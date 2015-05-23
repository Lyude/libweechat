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

#ifndef RELAY_EVENT_H
#define RELAY_EVENT_H

#include "relay.h"
#include "relay-parser.h"

#include <glib.h>

typedef void (*LibWCEventHandler)(LibWCRelay *relay,
                                  LibWCRelayMessage *event);

LibWCEventHandler _libwc_relay_event_get_handler(LibWCEventIdentifier id)
G_GNUC_INTERNAL G_GNUC_PURE;

#endif /* !RELAY_EVENT_H */
