/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_ASYNC_HELPERS_H
#define JUSTCAPTURE_ASYNC_HELPERS_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Run a synchronous function in a thread pool and complete the GTask. */
void just_capture_async_run_in_thread (GTask           *task,
                                       GTaskThreadFunc  func,
                                       gpointer         source_object,
                                       GCancellable    *cancellable);

G_END_DECLS

#endif /* JUSTCAPTURE_ASYNC_HELPERS_H */
