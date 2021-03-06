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

#ifndef MISC_H
#define MISC_H

#define LIBWC_CONSTRUCTOR __attribute__ ((constructor))

#define LIBWC_GET_FIELD(data_, offset_, type_) (*((type_*)(&((gint8*)data_)[offset_])))

#endif /* !MISC_H */
