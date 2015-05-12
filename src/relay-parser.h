/* ©2015 Stephen Chandler Paul <thatslyude@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#ifndef RELAY_PARSER_H
#define RELAY_PARSER_H

#include <glib.h>

typedef enum {
    LIBWC_NOT_AN_EVENT                  = -1,
    LIBWC_EVENT_BUFFER_OPENED           = 1,
    LIBWC_EVENT_BUFFER_TYPE_CHANGED     = 2,
    LIBWC_EVENT_BUFFER_MOVED            = 3,
    LIBWC_EVENT_BUFFER_MERGED           = 4,
    LIBWC_EVENT_BUFFER_UNMERGED         = 5,
    LIBWC_EVENT_BUFFER_HIDDEN           = 6,
    LIBWC_EVENT_BUFFER_UNHIDDEN         = 7,
    LIBWC_EVENT_BUFFER_RENAMED          = 8,
    LIBWC_EVENT_BUFFER_TITLE_CHANGED    = 9,
    LIBWC_EVENT_BUFFER_LOCALVAR_ADDED   = 10,
    LIBWC_EVENT_BUFFER_LOCALVAR_CHANGED = 11,
    LIBWC_EVENT_BUFFER_LOCALVAR_REMOVED = 12,
    LIBWC_EVENT_BUFFER_CLOSING          = 13,
    LIBWC_EVENT_BUFFER_CLEARED          = 14,
    LIBWC_EVENT_BUFFER_LINE_ADDED       = 15,
    LIBWC_EVENT_NICKLIST                = 16,
    LIBWC_EVENT_NICKLIST_DIFF           = 17,
    LIBWC_EVENT_PONG                    = 18,
    LIBWC_EVENT_UPGRADE                 = 19,
    LIBWC_EVENT_UPGRADE_ENDED           = 20
} LibWCEventIdentifier;

typedef enum {
    LIBWC_OBJECT_TYPE_CHAR      = 1,
    LIBWC_OBJECT_TYPE_INT       = 2,
    LIBWC_OBJECT_TYPE_LONG      = 3,
    LIBWC_OBJECT_TYPE_STRING    = 4,
    LIBWC_OBJECT_TYPE_BUFFER    = 5,
    LIBWC_OBJECT_TYPE_POINTER   = 6,
    LIBWC_OBJECT_TYPE_TIME      = 7,
    LIBWC_OBJECT_TYPE_HASHTABLE = 8,
    LIBWC_OBJECT_TYPE_HDATA     = 9,
    LIBWC_OBJECT_TYPE_INFO      = 10,
    LIBWC_OBJECT_TYPE_INFOLIST  = 11,
    LIBWC_OBJECT_TYPE_ARRAY     = 12
} LibWCRelayObjectType;

struct _LibWCRelayMessageObject {
    LibWCRelayObjectType type;
    GVariant *value;
};

typedef struct _LibWCRelayMessageObject LibWCRelayMessageObject;

struct _LibWCRelayMessage {
    LibWCEventIdentifier id;
    GList *objects;
};

typedef struct _LibWCRelayMessage LibWCRelayMessage;

LibWCRelayMessage *libwc_relay_message_parse_data(void *data,
                                                  gsize size,
                                                  GError **error);

#endif /* !RELAY_PARSER_H */
