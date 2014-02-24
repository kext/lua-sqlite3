ALL = sqlite3.so libsqlite3.a
.PHONY: clean install all Dependencies

PREFIX ?= /usr/local

all: $(ALL)

sqlite3.so: lua_sqlite3.o sqlite3.o
libsqlite3.a: lua_sqlite3.o sqlite3.o

CFLAGS = -O2 -Wall -pedantic
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
        LINKFLAGS = -bundle -undefined dynamic_lookup
else
        LINKFLAGS = -shared -fpic
endif

Dependencies:
	@$(CC) -MM $(CFLAGS) *.c > $@

-include Dependencies

%:
	@echo [LD] $@
	@$(CC) -o $@ -O $^

%.so:
	@echo [LD] $@
	@$(CC) -o $@ -O $(LINKFLAGS) $^

%.a:
	@echo [AR] $@
	@$(AR) rcs $@ $^

.c.o:
	@echo [CC] $@
	@$(CC) -o $@ -c $(CFLAGS) $<

clean:
	@rm -rf *.o *.so *.a *.dylib $(ALL) Dependencies

install: sqlite3.so
	@echo [Install]
	@install -d "$(PREFIX)/lib/lua/5.2"
	@install sqlite3.so "$(PREFIX)/lib/lua/5.2/sqlite3.so"
