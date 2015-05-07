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

#ifndef RELAY_MSG_H
#define RELAY_MSG_H

#include <glib.h>

typedef enum {
    LIBWC_CMD_BUFFER_OPENED,
    LIBWC_CMD_BUFFER_TYPE_CHANGED,
    LIBWC_CMD_BUFFER_MOVED,
    LIBWC_CMD_BUFFER_MERGED,
    LIBWC_CMD_BUFFER_UNMERGED,
    LIBWC_CMD_BUFFER_HIDDEN,
    LIBWC_CMD_BUFFER_UNHIDDEN,
    LIBWC_CMD_BUFFER_RENAMED,
    LIBWC_CMD_BUFFER_TITLE_CHANGED,
    LIBWC_CMD_BUFFER_LOCALVAR_ADDED,
    LIBWC_CMD_BUFFER_LOCALVAR_CHANGED,
    LIBWC_CMD_BUFFER_CLOSING,
    LIBWC_CMD_BUFFER_CLEARED,
    LIBWC_CMD_BUFFER_LINE_ADDED,
    LIBWC_CMD_NICKLIST,
    LIBWC_CMD_NICKLIST_DIFF,
    LIBWC_CMD_PONG,
    LIBWC_CMD_UPGRADE,
    LIBWC_CMD_UPGRADE_ENDED
} LibWCCommandIdentifier;

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
    LibWCCommandIdentifier id;
    GList *objects;
};

typedef struct _LibWCRelayMessage LibWCRelayMessage;

LibWCRelayMessage *libwc_relay_message_parse_data(void *data,
                                                  gsize size,
                                                  GError **error);

#endif /* !RELAY_MSG_H */
