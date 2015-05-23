AM_CFLAGS = -std=gnu11 $(GLIB_CFLAGS) $(GIO_CFLAGS) -I../src
AM_LDFLAGS = $(GLIB_LDFLAGS) $(GIO_LDFLAGS)
LDADD = ../src/libweechat.la

bin_PROGRAMS = test-parser \
               test-client

test_parser_SOURCES = test-parser.c
test_parser_LDFLAGS = -static # To access internal libweechat functions


test_client_SOURCES = test-client.c
