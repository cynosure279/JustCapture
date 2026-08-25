/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/async-helpers.h"

/**
 * just_capture_async_run_in_thread:
 * @task: a #GTask
 * @func: the thread function
 * @source_object: (nullable): source object for the task
 * @cancellable: (nullable): a #GCancellable
 *
 * Runs @func in a worker thread and completes @task when done.
 * Thin wrapper around g_task_run_in_thread().
 */
void
just_capture_async_run_in_thread (GTask           *task,
                                  GTaskThreadFunc  func,
                                  gpointer         source_object G_GNUC_UNUSED,
                                  GCancellable    *cancellable G_GNUC_UNUSED)
{
  g_return_if_fail (G_IS_TASK (task));

  g_task_run_in_thread (task, func);
}
