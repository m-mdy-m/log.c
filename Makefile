# m-mdy-m/log.c — build, test, install and package the standalone logger.
PROJECT := m-mdy-m/log.c
PACKAGE := m-mdy-m-log
VERSION := 0.1.0
LIBRARY := m-mdy-m-log
CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
DESTDIR ?=
CFLAGS ?= -O2 -g
CPPFLAGS ?=
WARNINGS := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
BASE_CFLAGS := -std=c11 $(WARNINGS) -Iinclude
OBJDIR := build/obj
LIBDIR := build/lib
BINDIR := build/bin
LUA_DIR := build/lua
SRC := src/log.c
OBJ := $(OBJDIR)/log.o
STATIC_LIB := $(LIBDIR)/lib$(LIBRARY).a
UNAME_S := $(shell uname -s 2>/dev/null || echo unknown)

ifeq ($(UNAME_S),Darwin)
SHARED_LIB := $(LIBDIR)/lib$(LIBRARY).1.dylib
SHARED_LINK_FLAGS := -dynamiclib -Wl,-install_name,@rpath/lib$(LIBRARY).1.dylib
LUA_RPATH := -Wl,-rpath,@loader_path/../lib
else
SHARED_LIB := $(LIBDIR)/lib$(LIBRARY).so.$(VERSION)
SHARED_LINK_FLAGS := -shared -Wl,-soname,lib$(LIBRARY).so.1
LUA_RPATH := -Wl,-rpath,'$$ORIGIN/../lib'
endif

.PHONY: all static shared test examples check docs-check abi-check e2e lua lua-check install uninstall clean package package-linux print-config
all: static shared

$(OBJDIR) $(LIBDIR) $(BINDIR) $(LUA_DIR):
	mkdir -p $@

$(OBJ): $(SRC) include/m-mdy-m/log.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) -fPIC -c $(SRC) -o $@

static: $(STATIC_LIB)
$(STATIC_LIB): $(OBJ) | $(LIBDIR)
	$(AR) rcs $@ $<

shared: $(SHARED_LIB)
$(SHARED_LIB): $(OBJ) | $(LIBDIR)
	$(CC) $(CFLAGS) $(SHARED_LINK_FLAGS) -o $@ $<
ifeq ($(UNAME_S),Darwin)
	ln -sf lib$(LIBRARY).1.dylib $(LIBDIR)/lib$(LIBRARY).dylib
else
	ln -sf lib$(LIBRARY).so.$(VERSION) $(LIBDIR)/lib$(LIBRARY).so.1
	ln -sf lib$(LIBRARY).so.1 $(LIBDIR)/lib$(LIBRARY).so
endif

$(BINDIR)/test_log: tests/test_log.c $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $< $(STATIC_LIB) -o $@

$(BINDIR)/test_threaded: tests/test_threaded.c $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $< $(STATIC_LIB) -pthread -o $@

$(BINDIR)/example_basic: examples/c/basic.c $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $< $(STATIC_LIB) -o $@

$(BINDIR)/example_abi_record: examples/c/abi_record.c $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $< $(STATIC_LIB) -o $@

test: $(BINDIR)/test_log $(BINDIR)/test_threaded
	./$(BINDIR)/test_log
	./$(BINDIR)/test_threaded

examples: $(BINDIR)/example_basic $(BINDIR)/example_abi_record

docs-check:
	python3 tools/check_docs.py

abi-check: shared
	python3 tests/test_check_abi_parser.py
	python3 tools/check_abi.py

# End-to-end: install to an isolated staging prefix and compile/run a new external consumer.
e2e: all
	rm -rf build/e2e-stage
	$(MAKE) DESTDIR="$(CURDIR)/build/e2e-stage" PREFIX=/usr/local install
	CC="$(CC)" bash tests/e2e_install.sh "$(CURDIR)/build/e2e-stage/usr/local"

check: all test examples docs-check abi-check e2e
	./$(BINDIR)/example_abi_record >/dev/null 2>&1

# Requires Lua 5.4 development headers and pkg-config, e.g. lua5.4 on Debian/Ubuntu.
LUA_PKG_CONFIG ?= lua5.4
lua: shared | $(LUA_DIR)
	@$(PKG_CONFIG) --exists $(LUA_PKG_CONFIG) || (echo "Missing $(LUA_PKG_CONFIG) pkg-config package; install Lua 5.4 development headers." >&2; exit 1)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) -fPIC -shared \
	  $$($(PKG_CONFIG) --cflags $(LUA_PKG_CONFIG)) bindings/lua/m_mdy_m_log_lua.c \
	  -L$(LIBDIR) -l$(LIBRARY) $(LUA_RPATH) \
	  $$($(PKG_CONFIG) --libs $(LUA_PKG_CONFIG)) -o $(LUA_DIR)/m_mdy_m_log.so

lua-check: lua
	LUA_CPATH='$(LUA_DIR)/?.so;;' LD_LIBRARY_PATH='$(LIBDIR):'$${LD_LIBRARY_PATH:-} lua5.4 examples/lua/basic.lua

# DESTDIR stages files and does not write to the actual system.
install: all
	install -d $(DESTDIR)$(PREFIX)/include/m-mdy-m $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 0644 include/m-mdy-m/log.h $(DESTDIR)$(PREFIX)/include/m-mdy-m/log.h
	install -m 0644 $(STATIC_LIB) $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).a
	if [ "$(UNAME_S)" = "Darwin" ]; then \
	  install -m 0755 $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).1.dylib; \
	  ln -sf lib$(LIBRARY).1.dylib $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).dylib; \
	else \
	  install -m 0755 $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so.$(VERSION); \
	  ln -sf lib$(LIBRARY).so.$(VERSION) $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so.1; \
	  ln -sf lib$(LIBRARY).so.1 $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so; \
	fi
	sed 's|@VERSION@|$(VERSION)|g' packaging/m-mdy-m-log.pc.in > $(DESTDIR)$(PREFIX)/lib/pkgconfig/m-mdy-m-log.pc

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/m-mdy-m/log.h
	rm -f $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).a $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so.1 $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).so.$(VERSION)
	rm -f $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).dylib $(DESTDIR)$(PREFIX)/lib/lib$(LIBRARY).1.dylib
	rm -f $(DESTDIR)$(PREFIX)/lib/pkgconfig/m-mdy-m-log.pc

package: check
	python3 tools/package_source.py --version $(VERSION) --outdir build/packages

package-linux: check
	python3 tools/package_linux.py --version $(VERSION) --outdir build/packages

print-config:
	@echo "project=$(PROJECT)"
	@echo "version=$(VERSION)"
	@echo "cc=$(CC)"
	@echo "prefix=$(PREFIX)"
	@echo "os=$(UNAME_S)"

clean:
	rm -rf build m-mdy-m-log-test-output.tmp service.log
