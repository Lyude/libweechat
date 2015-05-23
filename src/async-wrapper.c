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

#include "async-wrapper.h"

void
_libwc_blocking_task_init(LibWCBlockingTask *task) {
    g_cond_init(&task->cond);
    g_mutex_init(&task->mutex);

    g_mutex_lock(&task->mutex);
}

void
_libwc_blocking_task_wait_until_finish(LibWCBlockingTask *task) {
    g_cond_wait(&task->cond, &task->mutex);
    g_mutex_unlock(&task->mutex);
}

void
_libwc_unblock_on_finish(GObject *source_object,
                         GAsyncResult *res,
                         void *user_data) {
    LibWCBlockingTask *task = user_data;

    task->res = g_object_ref(res);

    g_cond_broadcast(&task->cond);
}
