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

#ifndef PRINTF_FORMAT_WRAPPERS_H
#define PRINTF_FORMAT_WRAPPERS_H

#define LIBWC_VPRINTF_WRAPPER(func_call)                      \
    gchar *printf_string;                                     \
                                                              \
    printf_string = g_strdup_vprintf(format_string, va_args); \
                                                              \
    func_call;                                                \
                                                              \
    g_free(printf_string);                                    \

#define LIBWC_PRINTF_WRAPPER(vprintf_func_call) \
    va_list va_args;                            \
                                                \
    va_start(va_args, format_string);           \
    vprintf_func_call;                          \
    va_end(va_args);                            \

#define LIBWC_VPRINTF_WRAPPER_RETURNS(func_call, return_type) \
    gchar *printf_string;                                     \
    return_type ret;                                          \
                                                              \
    printf_string = g_strdup_vprintf(format_string, va_args); \
                                                              \
    ret = func_call;                                          \
                                                              \
    g_free(printf_string);                                    \
                                                              \
    return ret;

#define LIBWC_PRINTF_WRAPPER_RETURNS(vprintf_func_call, return_type) \
    va_list va_args;                                                 \
    return_type ret;                                                 \
                                                                     \
    va_start(va_args, format_string);                                \
    ret = vprintf_func_call;                                         \
    va_end(va_args);                                                 \
                                                                     \
    return ret;

#endif /* !PRINTF_FORMAT_WRAPPERS_H */
