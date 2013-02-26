LIB = libhardware_legacy.wifi.so
OBJS = execs.o load_file.o wifi.o

SONAME = $(LIB)
CFLAGS = -Wall -std=c99 -Os -fPIC -fvisibility=hidden

ifneq ($(DEBUG),)
	CFLAGS += -DDEBUG
	LDFLAGS += -llog
endif

$(LIB): $(OBJS)
	$(CC) -shared -Wl,-z,interpose -Wl,-soname,$(SONAME) -o $(LIB) $(OBJS) $(LDFLAGS)

strip: $(LIB)
	strip $(LIB)

clean:
	rm -f $(LIB) $(OBJS)

.PHONY: strip clean
