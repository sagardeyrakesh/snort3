
x_luadir = $(pkgincludedir)/lua

noinst_LIBRARIES = libmanagers.a

x_lua_HEADERS = \
snort_config.lua \
snort_plugin.lua

libmanagers_a_SOURCES = \
action_manager.cc action_manager.h \
codec_manager.cc codec_manager.h \
connector_manager.cc connector_manager.h \
event_manager.cc event_manager.h \
inspector_manager.cc inspector_manager.h \
ips_manager.cc ips_manager.h \
lua_plugin_defs.h \
module_manager.cc module_manager.h \
mpse_manager.cc mpse_manager.h \
plugin_manager.cc plugin_manager.h \
script_manager.cc script_manager.h \
so_manager.cc so_manager.h

EXTRA_DIST = ffi_wrap.sh
CLEANFILES = snort_plugin.lua

snort_plugin.lua:
	$(srcdir)/ffi_wrap.sh $(srcdir)/lua_plugin_defs.h >$@
