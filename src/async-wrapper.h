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

#ifndef ASYNC_WRAPPER_H
#define ASYNC_WRAPPER_H

#include "libweechat.h"

#include <glib.h>
#include <gio/gio.h>

struct _LibWCBlockingTask {
    GCond cond;
    GMutex mutex;
    GAsyncResult *res;
};

typedef struct _LibWCBlockingTask LibWCBlockingTask;

void _libwc_blocking_task_init(LibWCBlockingTask *task)
G_GNUC_INTERNAL;

void _libwc_blocking_task_wait_until_finish(LibWCBlockingTask *task)
G_GNUC_INTERNAL;

void _libwc_unblock_on_finish(GObject *source_object,
                              GAsyncResult *res,
                              void *user_data)
G_GNUC_INTERNAL;

#define LIBWC_BLOCKING_WRAPPER(name, return_type, async_func, ...) \
    LibWCBlockingTask task;                                        \
    return_type result;                                            \
                                                                   \
    _libwc_blocking_task_init(&task);                              \
    async_func(relay, cancellable, _libwc_unblock_on_finish,       \
               &task, ##__VA_ARGS__);                              \
    _libwc_blocking_task_wait_until_finish(&task);                 \
                                                                   \
    result = name ## _finish(relay, task.res, error);              \
                                                                   \
    g_object_unref(task.res);                                      \
                                                                   \
    return result;

#endif /* !ASYNC_WRAPPER_H */
