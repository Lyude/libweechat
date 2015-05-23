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

#ifndef RELAY_PRIVATE_H
#define RELAY_PRIVATE_H

#include "relay.h"
#include "relay-connection.h"

#include <glib.h>
#include <gio/gio.h>

struct _LibWCRelayPrivate {
    GMainContext *context;
    GMainLoop *main_loop;
    GThread *thread;

    GSocket *socket;
    GSource *source;
    LibWCReadCallback read_cb;
    gsize read_len;

    GIOStream *stream;
    GInputStream *input_stream;
    GOutputStream *output_stream;
    GCancellable *input_stream_cancellable;

    gboolean connected;

    guint next_cmd_id;
    GZlibDecompressor *decompressor;

    GAsyncQueue *pending_writes;
    GHashTable *pending_tasks;
    GMutex pending_tasks_mutex;

    gchar *password;
};

#endif /* !RELAY_PRIVATE_H */
