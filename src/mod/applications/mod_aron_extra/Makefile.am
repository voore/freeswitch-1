include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_aron_extra

mod_LTLIBRARIES = mod_aron_extra.la
mod_aron_extra_la_SOURCES  = mod_aron_extra.c
mod_aron_extra_la_CFLAGS   = $(AM_CFLAGS)
mod_aron_extra_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_aron_extra_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
