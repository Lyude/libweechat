/* ©2015 Stephen Chandler Paul <thatslyude@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#include "../src/libweechat.h"

#include <glib.h>
#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define END_IF_FAIL(condition)                                        \
    if (!(condition)) {                                               \
        g_critical("%s: %s failed! Error returned: %s\n"              \
                   "Exiting\n",                                       \
                   G_STRLOC, G_STRINGIFY(condition), error->message); \
        exit(1);                                                      \
    }

#define TEST_PING_MESSAGE "Hello world!"

int main(int argc, char *argv[]) {
    LibWCRelay *relay = libwc_relay_new();
    GSocketClient *socket_client = g_socket_client_new();
    GSocketConnection *socket_connection;
    gboolean result;
    gchar *ping_result;
    GError *error = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: test-client <host[:port]>\n");
        exit(1);
    }

    socket_connection = g_socket_client_connect_to_host(socket_client, argv[1],
                                                        49153, NULL, &error);
    END_IF_FAIL(socket_connection != NULL);

    libwc_relay_password_set(relay, "test");
    libwc_relay_connection_set(relay, G_IO_STREAM(socket_connection),
                               g_socket_connection_get_socket(socket_connection));

    result = libwc_relay_connection_init(relay, NULL, &error);

    if (result)
        printf("Connection successful!\n");
    else {
        fprintf(stderr, "Connection unsuccessful: %s\n",
                error->message);
        exit(1);
    }

    ping_result = libwc_relay_ping(relay, NULL, &error, TEST_PING_MESSAGE);

    if (ping_result && strcmp(ping_result, TEST_PING_MESSAGE) == 0)
        printf("Ping successful!\n");
    else {
        fprintf(stderr, "Ping unsuccessful: %s\n",
                error->message);
        exit(1);
    }
}
