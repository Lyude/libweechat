AM_CFLAGS = -std=gnu11 $(GLIB_CFLAGS) $(GIO_CFLAGS)
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GIO_LDFLAGS)

lib_LTLIBRARIES = libweechat.la
libweechat_la_SOURCES = relay-msg.c
libweechat_la_LIBADD = $(GLIB_LIBS) $(GIO_LIBS)
