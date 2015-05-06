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
	message = libwc_relay_message_parse_data(data + 9, data_len - 9, &error);
	if (!message) {
		fprintf(stderr, "Failed to parse message: %s\n",
				error->message);
		exit(1);
	}

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
