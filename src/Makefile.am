AM_CFLAGS = -std=gnu11 $(GLIB_CFLAGS) $(GIO_CFLAGS)
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GIO_LDFLAGS)

lib_LTLIBRARIES = libweechat.la
libweechat_la_SOURCES = relay-parser.c     \
                        relay-event.c      \
                        relay-connection.c \
                        relay-command.c    \
                        relay.c            \
                        async-wrapper.c
libweechat_la_LIBADD = $(GLIB_LIBS) $(GIO_LIBS)

CLEANFILES =

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) \
                             --warn-all \
                             --identifier-prefix=LibWC \
                             --symbol-prefix=libwc
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
introspection_sources = $(libweechat_la_SOURCES)

LibWC-0.0.gir: libweechat.la
LibWC_0_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0 GModule-2.0
LibWC_0_0_gir_CFLAGS = $(INCLUDES)
LibWC_0_0_gir_LIBS = libweechat.la
LibWC_0_0_gir_FILES = $(introspection_sources) \
                  relay.h                  \
                  libweechat.h
INTROSPECTION_GIRS += LibWC-0.0.gir

girdir = $(datadir)/libweechat
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
