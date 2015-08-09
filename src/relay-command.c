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

#include "libweechat.h"
#include "async-wrapper.h"
#include "relay.h"
#include "relay-command.h"
#include "relay-connection.h"
#include "relay-private.h"
#include "printf-format-wrappers.h"

#include <glib.h>
#include <gio/gio.h>
#include <string.h>

guint
_libwc_command_id_new(LibWCRelay *relay) {
    guint new_id;

    g_mutex_lock(&relay->priv->pending_tasks_mutex);

    /* Starting from next_cmd_id, find the first ID that isn't already taken by
     * another pending task */
    for (new_id = relay->priv->next_cmd_id;
         g_hash_table_contains(relay->priv->pending_tasks,
                               GUINT_TO_POINTER(new_id)) || new_id == 0;
         new_id++);

    relay->priv->next_cmd_id = new_id + 1;

    g_mutex_unlock(&relay->priv->pending_tasks_mutex);

    return new_id;
}

void
_libwc_relay_pending_tasks_add(LibWCRelay *relay,
                               guint id,
                               GTask *task) {
    g_mutex_lock(&relay->priv->pending_tasks_mutex);
    g_hash_table_insert(relay->priv->pending_tasks, GUINT_TO_POINTER(id),
                        g_object_ref_sink(task));
    g_mutex_unlock(&relay->priv->pending_tasks_mutex);
}

void
_libwc_relay_pending_tasks_remove(LibWCRelay *relay,
                                  guint id) {
    g_mutex_lock(&relay->priv->pending_tasks_mutex);
    g_hash_table_remove(relay->priv->pending_tasks, GUINT_TO_POINTER(id));
    g_mutex_unlock(&relay->priv->pending_tasks_mutex);
}

GTask *
_libwc_relay_pending_tasks_lookup(LibWCRelay *relay,
                                  guint id) {
    GTask *task;

    g_mutex_lock(&relay->priv->pending_tasks_mutex);
    task = g_hash_table_lookup(relay->priv->pending_tasks, GUINT_TO_POINTER(id));
    g_mutex_unlock(&relay->priv->pending_tasks_mutex);

    return task;
}

/**
 * libwc_relay_ping_async:
 * @relay: a #LibWCRelay
 * @cancellable: (optional): option #GCancellable object, NULL to ignore
 * @callback: the function to call when we receive a response to the ping
 * @user_data: (closure): The data to pass to the callback
 * @ping_string: (optional): A character string to send with the ping
 *
 * Asynchronously sends a ping to the relay, optionally with a character string
 * attached.
 */
void
libwc_relay_ping_async(LibWCRelay *relay,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       void *user_data,
                       const gchar *ping_string) {
    guint id = _libwc_command_id_new(relay);
    GTask *task;

    task = g_task_new(relay, cancellable, callback, user_data);
    _libwc_relay_pending_tasks_add(relay, id, task);

    if (ping_string)
        _libwc_relay_connection_queue_cmd(relay, task, id, cancellable,
                                          "ping %x %s\n", id, ping_string);
    else
        _libwc_relay_connection_queue_cmd(relay, task, id, cancellable,
                                          "ping %x\n", id);
}

void
libwc_relay_pingv_async(LibWCRelay *relay,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        void *user_data,
                        const gchar *format_string,
                        va_list va_args) {
    LIBWC_VPRINTF_WRAPPER(libwc_relay_ping_async(relay, cancellable, callback,
                                                 user_data, printf_string));
}

void
libwc_relay_pingf_async(LibWCRelay *relay,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        void *user_data,
                        const gchar *format_string,
                        ...) {
    LIBWC_PRINTF_WRAPPER(libwc_relay_pingv_async(relay, cancellable, callback,
                                                 user_data, format_string,
                                                 va_args));
}

/**
 * libwc_relay_ping_finish:
 * @relay: a #LibWCRelay
 * @res: (out): a #GAsyncResult
 * @error: (out) (optional): a #Gerror location to store the error occuring, or
 * NULL to ignore
 *
 * Get the response to a ping that was sent to a weechat relay.
 *
 * Returns: The string attached to the ping. This is NULL if no string was sent
 * with the ping in the first place.
 */
gchar *
libwc_relay_ping_finish(LibWCRelay *relay,
                        GAsyncResult *res,
                        GError **error) {
    gchar *result;

    g_assert_null(*error);
    g_return_val_if_fail(g_task_is_valid(res, relay), FALSE);

    result = g_task_propagate_pointer(G_TASK(res), error);

    g_object_unref(res);

    return result;
}

/**
 * libwc_relay_ping:
 * @relay: a #LibWCRelay
 * @cancellable: (optional): option #GCancellable object, NULL to ignore
 * @error: (out) (optional): a #Gerror location to store the error occuring, or
 * NULL to ignore
 * @ping_string: (optional): A character string to send with the ping
 *
 * Synchronously send a ping to a weechat relay. This is a blocking version of
 * libwc_relay_ping_async().
 *
 * Returns: The string attached to the ping. This is NULL if no string was sent
 * with the ping in the first place.
 */
gchar *
libwc_relay_ping(LibWCRelay *relay,
                 GCancellable *cancellable,
                 GError **error,
                 const gchar *ping_string) {
    LIBWC_BLOCKING_WRAPPER(libwc_relay_ping, gchar*, libwc_relay_ping_async,
                           ping_string);
}

gchar *
libwc_relay_pingf(LibWCRelay *relay,
                  GCancellable *cancellable,
                  GError **error,
                  const gchar *format_string,
                  ...) {
    LIBWC_PRINTF_WRAPPER_RETURNS(libwc_relay_pingv(relay, cancellable, error,
                                                   format_string, va_args),
                                 gchar*);
}

gchar *
libwc_relay_pingv(LibWCRelay *relay,
                  GCancellable *cancellable,
                  GError **error,
                  const gchar *format_string,
                  va_list va_args) {
    LIBWC_VPRINTF_WRAPPER_RETURNS(libwc_relay_ping(relay, cancellable, error,
                                                   printf_string),
                                  gchar*);
}
