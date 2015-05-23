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
#include "relay-private.h"
#include "relay-parser.h"
#include "relay-connection.h"
#include "libweechat.h"

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/mman.h>

/* TODO:
 * - Eventually implement a synchronous version of initializing the relay. We're
 *   not going to be using it, but someone else might want to
 */

static void
async_initable_iface_init_async(GAsyncInitable *initable,
                                int io_priority,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                void *user_data);
static gboolean
async_initable_iface_init_finish(GAsyncInitable *initable,
                                 GAsyncResult *res,
                                 GError **error);

static void
libwc_relay_init_async_initable(GAsyncInitableIface *iface) {
}

G_DEFINE_TYPE_WITH_CODE(LibWCRelay, libwc_relay, G_TYPE_OBJECT,
                        G_ADD_PRIVATE(LibWCRelay);
                        G_IMPLEMENT_INTERFACE(G_TYPE_ASYNC_INITABLE,
                                              libwc_relay_init_async_initable));

static void
libwc_relay_class_init(LibWCRelayClass *klass) { }

static void
libwc_relay_init(LibWCRelay *self) {
    self->priv = libwc_relay_get_instance_private(self);
}

LibWCRelay *
libwc_relay_new() {
    LibWCRelay *relay;

    relay = LIBWC_RELAY(g_object_new(LIBWC_TYPE_RELAY, NULL));

    relay->priv->input_stream_cancellable = g_cancellable_new();
    relay->priv->pending_writes = g_async_queue_new();
    relay->priv->pending_tasks =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                              g_object_unref);
    relay->priv->decompressor =
        g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_ZLIB);

    g_mutex_init(&relay->priv->pending_tasks_mutex);

    return relay;
}

static void
async_initable_iface_init_async(GAsyncInitable *initable,
                                int io_priority,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                void *user_data) {
    LibWCRelay *relay = LIBWC_RELAY(initable);

    libwc_relay_connection_init_async(relay, cancellable, callback, user_data);
}

static gboolean
async_initable_iface_init_finish(GAsyncInitable *initable,
                                 GAsyncResult *res,
                                 GError **error) {
    LibWCRelay *relay = LIBWC_RELAY(initable);

    return libwc_relay_connection_init_finish(relay, res, error);
}

void
libwc_relay_password_set(LibWCRelay *relay,
                         const gchar *password) {
    gsize password_len;

    g_free(relay->priv->password);

    /* Copy the password by hand so that we can mlock the memory for it */
    password_len = strlen(password);
    relay->priv->password = g_new(char, password_len + 1);

    g_warn_if_fail(mlock(relay->priv->password, password_len + 1) == 0);

    strcpy(relay->priv->password, password);
}

void
libwc_relay_connection_set(LibWCRelay *relay,
                           GIOStream *stream,
                           GSocket *socket) {
    g_assert_false(relay->priv->connected);

    relay->priv->stream = g_object_ref(stream);
    relay->priv->input_stream = g_io_stream_get_input_stream(stream);
    relay->priv->output_stream = g_io_stream_get_output_stream(stream);

    relay->priv->socket = g_object_ref(socket);
}
