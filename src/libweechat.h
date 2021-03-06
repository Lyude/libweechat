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

#ifndef LIBWEECHAT_H
#define LIBWEECHAT_H

#include <glib.h>

#include "relay.h"

#define LIBWC_ERROR_RELAY (g_quark_from_static_string("libwc-relay-error"))

typedef enum {
    LIBWC_ERROR_RELAY_UNEXPECTED_EOM,
    LIBWC_ERROR_RELAY_INVALID_DATA
} LibWCRelayError;

#endif /* !LIBWEECHAT_H */
