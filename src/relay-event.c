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

#include "relay.h"
#include "relay-event.h"
#include "relay-command.h"
#include "relay-private.h"
#include "relay-parser.h"

#include <glib.h>
#include <stdlib.h>
#include <errno.h>

#define BEGIN_HANDLER __label__ event_error;

#define IGNORE_EVENT_IF_FAIL(condition_)                                    \
    if (G_UNLIKELY(!(condition_))) {                                        \
        g_warning("Received invalid event from relay (`%s` failed at %s), " \
                  "ignoring event",                                         \
                  G_STRINGIFY(condition_), G_STRLOC);                       \
        goto event_error;                                                   \
    }

static void
_libwc_event_handler_pong(LibWCRelay *relay,
                          LibWCRelayMessage *event) {
    BEGIN_HANDLER;
    LibWCRelayMessageObject *argument_object;
    const gchar *ping_msg;
    gchar **ping_args = NULL;
    guint command_id;
    GTask *pending_task;
    GVariant *maybe = NULL;

    /* The only data we should get, if any, is a string */
    argument_object = g_list_first(event->objects)->data;
    IGNORE_EVENT_IF_FAIL(argument_object->type == LIBWC_OBJECT_TYPE_STRING);

    maybe = g_variant_get_maybe(argument_object->value);
    IGNORE_EVENT_IF_FAIL(maybe != NULL);
    ping_msg = g_variant_get_string(maybe, NULL);

    /* Split the ping message into two parts: the message ID, and (if
     * applicable) whatever string was returned with the ping */
    ping_args = g_strsplit(ping_msg, " ", 2);

    errno = 0;
    command_id = strtoul(ping_args[0], NULL, 16);
    IGNORE_EVENT_IF_FAIL(errno == 0);

    pending_task = _libwc_relay_pending_tasks_lookup(relay, command_id);
    IGNORE_EVENT_IF_FAIL(pending_task != NULL);

    g_task_return_pointer(pending_task, ping_args[1], g_free);
    _libwc_relay_pending_tasks_remove(relay, command_id);

    /* Mark the second argument as NULL, so it doesn't get freed with the rest
     * of the arguments */
    ping_args[1] = NULL;

    g_variant_unref(maybe);
    g_strfreev(ping_args);

    return;

event_error:
    if (maybe)
        g_variant_unref(maybe);
    if (ping_args)
        g_strfreev(ping_args);

    return;
}

LibWCEventHandler
_libwc_relay_event_get_handler(LibWCEventIdentifier id) {
    LibWCEventHandler handler;

    switch (id) {
        case LIBWC_EVENT_PONG:
            handler = _libwc_event_handler_pong;
            break;
        default:
            handler = NULL;
            break;
    }

    return handler;
}
