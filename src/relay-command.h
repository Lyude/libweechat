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

#ifndef RELAY_COMMAND_H
#define RELAY_COMMAND_H

#include "libweechat.h"

#include <glib.h>

guint _libwc_command_id_new(LibWCRelay *relay)
G_GNUC_WARN_UNUSED_RESULT G_GNUC_INTERNAL;

void _libwc_relay_pending_tasks_add(LibWCRelay *relay,
                                    guint id,
                                    GTask *task)
G_GNUC_INTERNAL;

void _libwc_relay_pending_tasks_remove(LibWCRelay *relay,
                                       guint id)
G_GNUC_INTERNAL;

static inline gchar * _libwc_command_id_to_string(guint id)
G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

static inline gchar *
_libwc_command_id_to_string(guint id) {
    gchar *string = g_strdup_printf("%x", id);

    return string;
}

#endif /* !RELAY_COMMAND_H */
