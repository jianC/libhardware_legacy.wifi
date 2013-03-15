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

#define WIFI_DRIVER_MODULE_NAME "wlan"
#define WIFI_DRIVER_MODULE_PATH "/system/lib/modules/wlan.ko"
#define WIFI_DRIVER_MODULE_ARGS ""

#define DRIVER_PROP_NAME "wlan.driver.status"
#define AP_PROP_NAME "wlan.ap.driver.status"

static char *DRIVER_MODULE_NAME = WIFI_DRIVER_MODULE_NAME;
static char *DRIVER_MODULE_TAG = WIFI_DRIVER_MODULE_NAME " ";
static char *DRIVER_MODULE_PATH = WIFI_DRIVER_MODULE_PATH;
static char *DRIVER_MODULE_ARGS = WIFI_DRIVER_MODULE_ARGS;

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
