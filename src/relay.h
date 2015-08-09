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

#ifndef RELAY_H
#define RELAY_H

#include <glib-object.h>
#include <gio/gio.h>

#define LIBWC_TYPE_RELAY            (libwc_relay_get_type())
#define LIBWC_RELAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), LIBWC_TYPE_RELAY, LibWCRelay))
#define LIBWC_IS_RELAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIBWC_TYPE_RELAY))
#define LIBWC_RELAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), LIBWC_TYPE_RELAY, LibWCRelayClass))
#define LIBWC_IS_RELAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), LIBWC_TYPE_RELAY))
#define LIBWC_RELAY_GET_CLASS       (G_TYPE_INSTANCE_GET_CLASS((obj), LIBWC_TYPE_RELAY, LibWCRelayClass));

typedef struct _LibWCRelayPrivate LibWCRelayPrivate;

typedef struct _LibWCRelay {
    GObject parent_instance;

    /*< private >*/
    LibWCRelayPrivate *priv;
} LibWCRelay;

typedef struct _LibWCRelayClass {
    GObjectClass parent_class;
} LibWCRelayClass;

GType libwc_relay_get_type();

LibWCRelay * libwc_relay_new();

void libwc_relay_password_set(LibWCRelay *relay,
                              const gchar *password);

void libwc_relay_connection_set(LibWCRelay *relay,
                                GIOStream *stream,
                                GSocket *socket);

void libwc_relay_connection_init_async(LibWCRelay *relay,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       void *user_data);

gboolean libwc_relay_connection_init_finish(LibWCRelay *relay,
                                            GAsyncResult *res,
                                            GError **error);

gboolean libwc_relay_connection_init(LibWCRelay *relay,
                                     GCancellable *cancellable,
                                     GError **error);

void libwc_relay_ping_async(LibWCRelay *relay,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            void *user_data,
                            const gchar *ping_string);

void libwc_relay_pingf_async(LibWCRelay *relay,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             void *user_data,
                             const gchar *format_string,
                             ...)
G_GNUC_PRINTF(5, 6);

void libwc_relay_pingv_async(LibWCRelay *relay,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             void *user_data,
                             const gchar *format_string,
                             va_list args);

gchar * libwc_relay_ping_finish(LibWCRelay *relay,
                                GAsyncResult *res,
                                GError **error);

gchar * libwc_relay_ping(LibWCRelay *relay,
                         GCancellable *cancellable,
                         GError **error,
                         const gchar *ping_string);

gchar * libwc_relay_pingf(LibWCRelay *relay,
                          GCancellable *cancellable,
                          GError **error,
                          const gchar *format_string,
                          ...)
G_GNUC_PRINTF(4, 5);

gchar * libwc_relay_pingv(LibWCRelay *relay,
                          GCancellable *cancellable,
                          GError **error,
                          const gchar *format_string,
                          va_list args);

#endif /* !RELAY_H */
