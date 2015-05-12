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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/* This is the test message from the weechat documentation with the header
 * stripped */
static char *
wc_type_to_string(LibWCRelayObjectType type) {
    switch (type) {
        case LIBWC_OBJECT_TYPE_CHAR:
            return "Character";
        case LIBWC_OBJECT_TYPE_INT:
            return "Integer";
        case LIBWC_OBJECT_TYPE_LONG:
            return "Long";
        case LIBWC_OBJECT_TYPE_STRING:
            return "String";
        case LIBWC_OBJECT_TYPE_BUFFER:
            return "Buffer";
        case LIBWC_OBJECT_TYPE_POINTER:
            return "Pointer";
        case LIBWC_OBJECT_TYPE_TIME:
            return "Time";
        case LIBWC_OBJECT_TYPE_HASHTABLE:
            return "Hashtable";
        case LIBWC_OBJECT_TYPE_HDATA:
            return "Hdata";
        case LIBWC_OBJECT_TYPE_INFO:
            return "Info";
        case LIBWC_OBJECT_TYPE_INFOLIST:
            return "Infolist";
        case LIBWC_OBJECT_TYPE_ARRAY:
            return "Array";
        default:
            return "???";
    }
}

static char *
wc_cmd_id_to_string(LibWCEventIdentifier id) {
    switch (id) {
        case LIBWC_EVENT_BUFFER_OPENED:
            return "Buffer opened";
        case LIBWC_EVENT_BUFFER_TYPE_CHANGED:
            return "Buffer type changed";
        case LIBWC_EVENT_BUFFER_MOVED:
            return "Buffer moved";
        case LIBWC_EVENT_BUFFER_MERGED:
            return "Buffer merged";
        case LIBWC_EVENT_BUFFER_UNMERGED:
            return "Buffer unmerged";
        case LIBWC_EVENT_BUFFER_HIDDEN:
            return "Buffer hidden";
        case LIBWC_EVENT_BUFFER_UNHIDDEN:
            return "Buffer unhidden";
        case LIBWC_EVENT_BUFFER_RENAMED:
            return "Buffer renamed";
        case LIBWC_EVENT_BUFFER_TITLE_CHANGED:
            return "Buffer title changed";
        case LIBWC_EVENT_BUFFER_LOCALVAR_ADDED:
            return "Buffer localvar added";
        case LIBWC_EVENT_BUFFER_LOCALVAR_CHANGED:
            return "Buffer localvar changed";
        case LIBWC_EVENT_BUFFER_LOCALVAR_REMOVED:
            return "Buffer localvar removed";
        case LIBWC_EVENT_BUFFER_CLOSING:
            return "Buffer closing";
        case LIBWC_EVENT_BUFFER_CLEARED:
            return "Buffer cleared";
        case LIBWC_EVENT_BUFFER_LINE_ADDED:
            return "Buffer line added";
        case LIBWC_EVENT_NICKLIST:
            return "Nicklist";
        case LIBWC_EVENT_NICKLIST_DIFF:
            return "Nicklist diff";
        case LIBWC_EVENT_PONG:
            return "Pong";
        case LIBWC_EVENT_UPGRADE:
            return "Upgrade";
        case LIBWC_EVENT_UPGRADE_ENDED:
            return "Upgrade ended";
        default:
            return "???";
    }
}

int main(int argc, char *argv[]) {
    GIOChannel *stdin_channel;
    gchar *data;
    gsize data_len;
    GIOStatus read_status;
    LibWCRelayMessage *message;
    GError *error = NULL;

    if (argc < 1) {
        fprintf(stderr, "Usage: test_parser <message_data>\n");
        exit(1);
    }

    g_return_val_if_fail(g_file_get_contents(argv[1], &data, &data_len, &error),
                         -1);

    /* We start at 5 bytes after data so that we can skip the header */
    message = libwc_relay_message_parse_data(data + 5, data_len - 5, &error);
    if (!message) {
        fprintf(stderr, "Failed to parse message: %s\n",
                error->message);
        exit(1);
    }

    if (message->id)
        printf("Message ID: %s\n",
               wc_cmd_id_to_string(message->id));
    else
        printf("Message ID: None\n");

    for (GList *l = message->objects; l != NULL; l = l->next) {
        LibWCRelayMessageObject *object = l->data;
        GVariant *value;

        printf("Type: %s\n"
               "Value: %s\n",
               wc_type_to_string(object->type),
               g_variant_print(object->value, TRUE));
    }

    return 0;
}
