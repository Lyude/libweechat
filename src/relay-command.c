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

void
libwc_relay_ping_async(LibWCRelay *relay,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       void *user_data,
                       const gchar *ping_string) {
    guint id = _libwc_command_id_new(relay);
    gchar *command_string;
    GBytes *command_data;
    GTask *task;

    task = g_task_new(relay, cancellable, callback, user_data);
    _libwc_relay_pending_tasks_add(relay, id, task);

    if (ping_string)
        command_string = g_strdup_printf("ping %x %s\n",
                                         id, ping_string);
    else
        command_string = g_strdup_printf("ping %x\n",
                                         id);

    command_data = g_bytes_new_take(command_string, strlen(command_string));
    _libwc_relay_connection_queue_command(relay, command_data, task, id,
                                          cancellable);

    g_bytes_unref(command_data);
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
