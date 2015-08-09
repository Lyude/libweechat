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
#include "relay-private.h"
#include "relay-connection.h"
#include "relay-command.h"
#include "relay-parser.h"
#include "relay-event.h"
#include "misc.h"

#include <glib.h>
#include <gio/gio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdarg.h>

#define HEADER_SIZE ((gsize)5)

#define PAYLOAD_SIZE_OFFSET             (0)
#define PAYLOAD_COMPRESSION_FLAG_OFFSET (4)

struct _LibWCQueuedWrite {
    LibWCRelay *relay;
    GCancellable *cancellable;
    gulong cancellable_id;
    GBytes *data;
};

typedef struct _LibWCQueuedWrite LibWCQueuedWrite;

static void
queued_write_free(LibWCQueuedWrite *queued_write) {
    g_bytes_unref(queued_write->data);
    g_object_unref(queued_write->relay);

    if (queued_write->cancellable)
        g_object_unref(queued_write->cancellable);

    g_free(queued_write);
}

static LibWCQueuedWrite *
queued_write_new(LibWCRelay *relay,
                 GBytes *data,
                 GTask *task,
                 guint id,
                 GCancellable *cancellable) {
    LibWCQueuedWrite *queued_write = g_new0(LibWCQueuedWrite, 1);

    if (cancellable) {
        queued_write->cancellable = g_object_ref(cancellable);
        queued_write->cancellable_id =
            g_cancellable_connect(cancellable,
                                  G_CALLBACK(queued_write_free),
                                  queued_write, NULL);
    }

    if (task) {
        if (!id)
            id = _libwc_command_id_new(relay);

        _libwc_relay_pending_tasks_add(relay, id, task);
    }

    *queued_write = (LibWCQueuedWrite) {
        .relay = g_object_ref(relay),
        .data = g_bytes_ref(data),
    };

    return queued_write;
}

void
_libwc_relay_connection_end_on_error(LibWCRelay *relay,
                                     GError *error) {
    GList *pending_tasks;

    relay->priv->connected = FALSE;

    pending_tasks = g_hash_table_get_values(relay->priv->pending_tasks);

    for (GList *l = pending_tasks; l != NULL; l = l->next) {
        GTask *task = l->data;
        GCancellable *cancellable;

        cancellable = g_task_get_cancellable(task);
        g_cancellable_cancel(cancellable);
    }

    g_io_stream_clear_pending(relay->priv->stream);
    g_io_stream_close(relay->priv->stream, NULL, NULL);

    g_hash_table_remove_all(relay->priv->pending_tasks);
    g_async_queue_unref(relay->priv->pending_writes);
}

static void
read_msg_header_cb(LibWCRelay *relay,
                   void *data,
                   gsize count);

static void
read_payload_cb(LibWCRelay *relay,
                void *data,
                gsize count) {
    LibWCRelayMessage *parsed_message;
    GError *error = NULL;
    LibWCEventHandler event_handler;

    parsed_message = _libwc_relay_message_parse_data(data, count, &error);
    if (G_UNLIKELY(!parsed_message)) {
        _libwc_relay_connection_end_on_error(relay, error);
        return;
    }

    if (parsed_message->type == LIBWC_RELAY_MESSAGE_TYPE_EVENT) {
        event_handler = _libwc_relay_event_get_handler(parsed_message->event_id);
        event_handler(relay, parsed_message);
    }

    _libwc_relay_message_free(parsed_message);

    relay->priv->read_cb = read_msg_header_cb;
    relay->priv->read_len = HEADER_SIZE;
}

static void
read_compressed_payload_cb(LibWCRelay *relay,
                           void *data,
                           gsize count) {
    GConverterResult result;
    void *outbuf = NULL;
    gsize outbuf_size = count,
          bytes_read = 0,
          bytes_written = 0;
    GError *error = NULL;

    do {
        outbuf_size *= 2;
        outbuf = g_realloc(outbuf, outbuf_size);

        result = g_converter_convert(G_CONVERTER(relay->priv->decompressor),
                                     data, count, outbuf, outbuf_size,
                                     G_CONVERTER_NO_FLAGS, &bytes_read,
                                     &bytes_written, &error);
    } while (result == G_CONVERTER_CONVERTED);

    g_converter_reset(G_CONVERTER(relay->priv->decompressor));

    if (result != G_CONVERTER_FINISHED) {
        _libwc_relay_connection_end_on_error(relay, error);
        g_free(outbuf);

        return;
    }

    read_payload_cb(relay, outbuf, outbuf_size);
    g_free(outbuf);
}

static void
read_msg_header_cb(LibWCRelay *relay,
                   void *data,
                   gsize count) {
    gsize payload_size =
        GINT32_FROM_BE(LIBWC_GET_FIELD(data, PAYLOAD_SIZE_OFFSET, guint32)) -
        HEADER_SIZE;

    /* Use the zlib payload callback if the compression flag is on in the
     * header */
    if (LIBWC_GET_FIELD(data, PAYLOAD_COMPRESSION_FLAG_OFFSET, guint8))
        relay->priv->read_cb = read_compressed_payload_cb;
    else
        relay->priv->read_cb = read_payload_cb;

    relay->priv->read_len = payload_size;
}

static void
queued_write_cb(GObject *source_object,
                GAsyncResult *res,
                void *user_data) {
    LibWCQueuedWrite *queued_write = user_data,
                     *next_write;
    GOutputStream *stream = G_OUTPUT_STREAM(source_object);
    GAsyncQueue *queued_writes = queued_write->relay->priv->pending_writes;
    gssize bytes_written;
    gsize data_size;
    GBytes *new_data;
    GError *error = NULL;

    bytes_written = g_output_stream_write_bytes_finish(stream, res, &error);

    if (error) {
        _libwc_relay_connection_end_on_error(queued_write->relay, error);
        return;
    }

    data_size = g_bytes_get_size(queued_write->data);
    if (bytes_written < data_size) {
        new_data = g_bytes_new_from_bytes(queued_write->data, bytes_written,
                                          data_size - bytes_written);

        g_bytes_unref(queued_write->data);
        queued_write->data = new_data;

        g_output_stream_write_bytes_async(
            queued_write->relay->priv->output_stream, new_data,
            G_PRIORITY_DEFAULT, queued_write->cancellable,
            queued_write_cb, queued_write);
        return;
    }

    g_async_queue_lock(queued_writes);
    g_async_queue_pop_unlocked(queued_writes);

    next_write = g_async_queue_try_pop_unlocked(queued_writes);
    if (next_write) {
        g_output_stream_write_bytes_async(
            queued_write->relay->priv->output_stream, next_write->data,
            G_PRIORITY_DEFAULT, next_write->cancellable,
            queued_write_cb, next_write);

        g_async_queue_push_unlocked(queued_writes, next_write);
    }

    g_async_queue_unlock(queued_writes);
    queued_write_free(queued_write);
}

void
queue_write(LibWCRelay *relay,
            LibWCQueuedWrite *write) {
    gsize queue_length;

    g_async_queue_lock(relay->priv->pending_writes);
    queue_length = g_async_queue_length_unlocked(relay->priv->pending_writes);

    g_async_queue_push_unlocked(relay->priv->pending_writes, write);
    g_async_queue_unlock(relay->priv->pending_writes);

    if (queue_length <= 0)
        g_output_stream_write_bytes_async(relay->priv->output_stream,
                                          write->data, G_PRIORITY_DEFAULT,
                                          write->cancellable,
                                          queued_write_cb, write);
}

void
_libwc_relay_connection_queue_cmd(LibWCRelay *relay,
                                  GTask *task,
                                  guint id,
                                  GCancellable *cancellable,
                                  const gchar *format,
                                  ...) {
    LibWCQueuedWrite *queued_write;
    gchar *cmd_string;
    GBytes *data;
    va_list ap;

    va_start(ap, format);
    cmd_string = g_strdup_vprintf(format, ap);
    va_end(ap);

    data = g_bytes_new_take(cmd_string, strlen(cmd_string) + 1);
    queued_write = queued_write_new(relay, data, task, id, cancellable);

    queue_write(relay, queued_write);
}

void
_libwc_relay_connection_queue_cmd_static(LibWCRelay *relay,
                                         GTask *task,
                                         guint id,
                                         GCancellable *cancellable,
                                         const gchar *command,
                                         gsize len) {
    LibWCQueuedWrite *queued_write;
    GBytes *data;

    data = g_bytes_new_static(command, len);
    queued_write = queued_write_new(relay, data, task, id, cancellable);
    queue_write(relay, queued_write);
}

static void
relay_connection_init_ping_response_cb(GObject *source_object,
                                       GAsyncResult *res,
                                       void *user_data) {
    LibWCRelay *relay = LIBWC_RELAY(source_object);
    GTask *task = user_data;
    GError *error = NULL;

    libwc_relay_ping_finish(relay, res, &error);

    if (error)
        g_task_return_error(task, error);
    else
        g_task_return_boolean(task, TRUE);

    g_object_unref(task);
}

gboolean
socket_source_cb(GSocket *socket,
                 GIOCondition condition,
                 void *user_data) {
    __label__ socket_error;
    LibWCRelay *relay = user_data;
    GError *error = NULL;
    void *data;
    gsize count;

    if (condition & (G_IO_ERR | G_IO_HUP))
        goto socket_error;

    data = g_malloc(relay->priv->read_len);

    g_input_stream_read_all(relay->priv->input_stream, data,
                            relay->priv->read_len, &count,
                            relay->priv->input_stream_cancellable, &error);

    if (error)
        goto socket_error;

    relay->priv->read_cb(relay, data, count);
    g_free(data);

    return TRUE;

socket_error:
    g_socket_shutdown(socket, TRUE, TRUE, &error);
    _libwc_relay_connection_end_on_error(relay, error);
    g_main_loop_quit(relay->priv->main_loop);
    g_free(data);

    return FALSE;
}

static void
relay_connection_init_async_worker(GTask *task) {
    LibWCRelay *relay = LIBWC_RELAY(g_task_get_source_object(task));
    GCancellable *cancellable = g_task_get_cancellable(task);

    relay->priv->context = g_main_context_ref_thread_default();

    relay->priv->source =
        g_socket_create_source(relay->priv->socket, G_IO_IN | G_IO_PRI,
                               relay->priv->input_stream_cancellable);

    g_source_set_callback(relay->priv->source, (GSourceFunc)socket_source_cb,
                          relay, NULL);
    g_source_attach(relay->priv->source, relay->priv->context);

    relay->priv->read_cb = read_msg_header_cb;
    relay->priv->read_len = HEADER_SIZE;

    if (relay->priv->password)
        _libwc_relay_connection_queue_cmd(relay, NULL, 0, cancellable,
                                          "init password=%s\n",
                                          relay->priv->password);
    else
        _libwc_relay_connection_queue_cmd_static(relay, NULL, 0, cancellable,
                                                 "init\n", sizeof("init\n"));

    /* The ping command won't work if the previous init command fails, so we can
     * use it to check whether or not we've successfully initialized */
    libwc_relay_ping_async(relay, cancellable,
                           relay_connection_init_ping_response_cb, task, NULL);

    relay->priv->main_loop = g_main_loop_new(relay->priv->context, FALSE);
    g_main_loop_run(relay->priv->main_loop);
}

/**
 * libwc_relay_connection_init_async:
 * @relay: a #LibWCRelay
 * @cancellable: (optional): optional #GCancellable object, NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): The data to pass to the callback
 *
 * Asynchronously connect to a Weechat relay.
 */
void
libwc_relay_connection_init_async(LibWCRelay *relay,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  void *user_data) {
    GTask *init_task;

    g_assert_nonnull(relay->priv->stream);
    g_assert_nonnull(relay->priv->socket);
    g_assert_false(g_io_stream_is_closed(relay->priv->stream));
    g_assert_false(g_output_stream_has_pending(relay->priv->output_stream));

    init_task = g_task_new(relay, cancellable, callback, user_data);

    if (cancellable)
        g_task_set_check_cancellable(init_task, TRUE);

    relay->priv->thread = g_thread_new(
        "libweechat", (GThreadFunc)relay_connection_init_async_worker,
        init_task);
}

/**
 * libwc_relay_connection_init_finish:
 * @relay: a #LibWCRelay
 * @res: (out): a #GAsyncResult
 * @error: (out) (optional): a #Gerror location to store the error occuring, or
 * NULL to ignore
 *
 * Get the results of an attempt to connect asynchronously to a Weechat relay.
 *
 * Returns: TRUE on success, FALSE if an error occured
 */
gboolean
libwc_relay_connection_init_finish(LibWCRelay *relay,
                                   GAsyncResult *res,
                                   GError **error) {
    gboolean result;

    g_assert_null(*error);
    g_return_val_if_fail(g_task_is_valid(res, relay), FALSE);

    result = g_task_propagate_boolean(G_TASK(res), error);

    return result;
}

/**
 * libwc_relay_connection_init:
 * @relay: a #LibWCRelay
 * @cancellable: (optional): option #GCancellable object, NULL to ignore
 * @error: (out) (optional): a #Gerror location to store the error occuring, or
 * NULL to ignore
 *
 * A synchronous, blocking version of libwc_relay_connection_init()
 *
 * Returns: TRUE on success, FALSE if an error occured
 */
gboolean
libwc_relay_connection_init(LibWCRelay *relay,
                            GCancellable *cancellable,
                            GError **error) {
    LIBWC_BLOCKING_WRAPPER(libwc_relay_connection_init, gboolean,
                           libwc_relay_connection_init_async);
}
