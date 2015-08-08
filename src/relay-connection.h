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

#ifndef RELAY_CONNECTION_H
#define RELAY_CONNECTION_H

#include "libweechat.h"

typedef void (*LibWCReadCallback) (LibWCRelay *relay,
                                   void *data,
                                   gsize size);

void _libwc_relay_connection_end_on_error(LibWCRelay *relay,
                                          GError *error)
G_GNUC_INTERNAL;

void _libwc_relay_connection_queue_cmd(LibWCRelay *relay,
                                       GTask *task,
                                       guint id,
                                       GCancellable *cancellable,
                                       const gchar *format,
                                       ...)
G_GNUC_INTERNAL;

void _libwc_relay_connection_queue_cmd_static(LibWCRelay *relay,
                                              GTask *task,
                                              guint id,
                                              GCancellable *cancellable,
                                              const gchar *command,
                                              gsize len)
G_GNUC_INTERNAL;

#endif /* !RELAY_CONNECTION_H */
