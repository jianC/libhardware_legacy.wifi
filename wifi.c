#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <android/log.h>
#include <sys/system_properties.h>

#include "execs.h"
#include "load_file.h"

#ifdef DEBUG
#define debug_log(prio, msg) __android_log_write(prio, "libhardware_legacy_wifi", msg)
#define debug_printf(prio, fmt, ...) __android_log_print(prio, "libhardware_legacy_wifi", fmt, __VA_ARGS__)
#else
static inline void debug_log(int prio, char *msg) {
}
static inline void debug_printf(int prio, char *fmt, ...) {
}
#endif

#define PUBLIC_API __attribute__ (( visibility("default") ))

#define WIFI_DRIVER_MODULE_NAME "tiwlan_drv"
#define WIFI_DRIVER_MODULE_PATH "/system/lib/modules/tiwlan_drv.ko"
#define WIFI_DRIVER_MODULE_ARGS ""
#define WIFI_AP_MODULE_ARGS ""
#define WIFI_FIRMWARE_LOADER "wlan_loader"

#define DRIVER_PROP_NAME "wlan.driver.status"
#define AP_PROP_NAME "wlan.ap.driver.status"

#define WIFI_GET_FW_PATH_STA 0
#define WIFI_GET_FW_PATH_AP 1
#define WIFI_GET_FW_PATH_P2P 2
#define WIFI_FW_PATH_STA "system/etc/wifi/fw_wlan1271.bin"
#define WIFI_FW_PATH_AP "/system/etc/wifi/fw_tiwlan_ap.bin"

static char *DRIVER_MODULE_NAME = WIFI_DRIVER_MODULE_NAME;
static char *DRIVER_MODULE_TAG = WIFI_DRIVER_MODULE_NAME " ";
static char *DRIVER_MODULE_PATH = WIFI_DRIVER_MODULE_PATH;
static char *DRIVER_MODULE_ARGS = WIFI_DRIVER_MODULE_ARGS;
static char *AP_MODULE_ARGS = WIFI_AP_MODULE_ARGS;
static char *FIRMWARE_LOADER = WIFI_FIRMWARE_LOADER;

extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static inline int insmod(char *modpath, char *modargs);
static inline int rmmod(char *module);
static inline int property_set(char *name, char *value);

int PUBLIC_API is_wifi_driver_loaded() {
	FILE *proc;
	char line[sizeof(DRIVER_MODULE_TAG)+10];

	if ((proc = fopen("/proc/modules", "r")) == NULL) {
		property_set(DRIVER_PROP_NAME, "unloaded");
		return 0;
	}
	while ((fgets(line, sizeof(line), proc)) != NULL) {
		if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
			property_set(DRIVER_PROP_NAME, "ok");
			fclose(proc);
			return 1;
		}
	}
	fclose(proc);
	property_set(DRIVER_PROP_NAME, "unloaded");
	return 0;
}

int PUBLIC_API wifi_unload_driver() {
	usleep(200000);
	if (rmmod(DRIVER_MODULE_NAME) == 0) {
		int count = 20;
		while (count-- > 0) {
			if (!is_wifi_driver_loaded())
				break;
			usleep(500000);
		}
		usleep(500000);
		if (count) {
			return 0;
		}
		return -1;
	} else {
		if (!is_wifi_driver_loaded())
			return 0;
		return -1;
	}
}

int PUBLIC_API wifi_load_driver() {
	int result;

	if (is_wifi_driver_loaded()) {
		return 0;
	}

	property_set(DRIVER_PROP_NAME, "loading");

	if (strcmp(FIRMWARE_LOADER,"wlan_loader") != 0) {
		property_set("ctl.start", FIRMWARE_LOADER);
	}

	result = insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARGS);
	if (result == 0) {
		property_set(DRIVER_PROP_NAME, "ok");
		return 0;
	}
	property_set(DRIVER_PROP_NAME, "timeout");
	wifi_unload_driver();
	return -1;
}

int PUBLIC_API is_wifi_hotspot_driver_loaded() {
	FILE *proc;
	char line[sizeof(DRIVER_MODULE_TAG)+10];

	if ((proc = fopen("/proc/modules", "r")) == NULL) {
		property_set(AP_PROP_NAME, "unloaded");
		return 0;
	}
	while ((fgets(line, sizeof(line), proc)) != NULL) {
		if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
			property_set(AP_PROP_NAME, "ok");
			fclose(proc);
			return 1;
		}
	}
	fclose(proc);
	property_set(AP_PROP_NAME, "unloaded");
	return 0;
}

int PUBLIC_API wifi_unload_hotspot_driver() {
	usleep(200000);
	if (rmmod(DRIVER_MODULE_NAME) == 0) {
		int count = 20;
		while (count-- > 0) {
			if (!is_wifi_hotspot_driver_loaded())
				break;
			usleep(500000);
		}
		usleep(500000);
		if (count) {
			return 0;
		}
		return -1;
	} else {
		if (!is_wifi_hotspot_driver_loaded())
			return 0;
		return -1;
	}
}

int PUBLIC_API wifi_load_hotspot_driver() {
	int result;

	if (is_wifi_hotspot_driver_loaded()) {
		return 0;
	}

	property_set(AP_PROP_NAME, "loading");

	result = insmod(DRIVER_MODULE_PATH, AP_MODULE_ARGS);
	if (result == 0) {
		property_set(AP_PROP_NAME, "ok");
		return 0;
	}
	property_set(AP_PROP_NAME, "timeout");
	wifi_unload_hotspot_driver();
	return -1;
}

int PUBLIC_API wifi_change_fw_path(const char *fwpath) {
	char val[PROP_VALUE_MAX];

	if (!fwpath)
		return 0;

	/*
	 * XXX EPIC HACK
	 * For newer drivers, wifi_change_fw_path() has the effect of switching
	 * the wlan driver between STA and AP mode, so do the same here (even
	 * though the actions have little to do with firmware paths).
	 */
	if (strcmp(fwpath, WIFI_FW_PATH_STA) == 0) {
		__system_property_get(DRIVER_PROP_NAME, val);
		if (strcmp(val, "ok") != 0) {
			debug_log(ANDROID_LOG_DEBUG, "Switching to STA mode from AP mode");
			/* Need to switch modes */
			wifi_unload_hotspot_driver();
			wifi_load_driver();
		}
	} else if (strcmp(fwpath, WIFI_FW_PATH_AP) == 0) {
		__system_property_get(AP_PROP_NAME, val);
		if (strcmp(val, "ok") != 0) {
			debug_log(ANDROID_LOG_DEBUG, "Switching to AP mode from STA mode");
			/* Need to switch modes */
			wifi_unload_driver();
			wifi_load_hotspot_driver();
		}
	} else {
		debug_log(ANDROID_LOG_DEBUG, "FW path doesn't match any known opmode!");
	}

	return 0;
}

char PUBLIC_API *wifi_get_fw_path(int fw_type) {
	switch (fw_type) {
	case WIFI_GET_FW_PATH_AP:
		debug_printf(ANDROID_LOG_DEBUG, "wifi_get_fw_path gives FW path %s", WIFI_FW_PATH_AP);
		/*
		 * XXXXXX EPIC EPIC HACK
		 * For some reason, wifi_change_fw_path() doesn't seem
		 * to be called by the framework, so we cheat and call it here.
		 */
		wifi_change_fw_path(WIFI_FW_PATH_AP);
		return WIFI_FW_PATH_AP;
		break;
	case WIFI_GET_FW_PATH_STA:
		debug_printf(ANDROID_LOG_DEBUG, "wifi_get_fw_path gives FW path %s", WIFI_FW_PATH_STA);
		/*
		 * XXXXXX EPIC EPIC HACK
		 * For some reason, wifi_change_fw_path() doesn't seem
		 * to be called by the framework, so we cheat and call it here.
		 */
		wifi_change_fw_path(WIFI_FW_PATH_STA);
		return WIFI_FW_PATH_STA;
		break;
	}
	debug_printf(ANDROID_LOG_DEBUG, "%d: unknown fw type!", fw_type);
	return NULL;
}

static inline int insmod(char *modpath, char *modargs) {
	void *module;
	size_t size;
	int result;

	if (!modpath)
		return -1;

	if (!modargs)
		modargs = "";

	module = load_file(modpath, &size);
	if (!module)
		return -1; 
	result = init_module(module, size, modargs); 
	free(module);

	return result;
}

static inline int rmmod(char *module) {
	int maxtries = 10;
	int result;

	if (!module)
		return -1;

	while (maxtries-- > 0) {
		result = delete_module(module, O_NONBLOCK | O_EXCL | O_TRUNC);
		if (result == -1 && errno == EAGAIN)
			usleep(500000);
		else
			break;
	}

	return result;
}

static inline int property_set(char *name, char *value) {
	if (!name || !value)
		return -1;

	return execs("/system/bin/setprop", 2, name, value);
}
