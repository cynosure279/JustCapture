/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_SCREENSHOT_PORTAL_H
#define JUSTCAPTURE_SCREENSHOT_PORTAL_H

#include <gio/gio.h>
#include "justcapture/portal-client.h"
#include "justcapture/types.h"
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Query available screenshot targets asynchronously. */
void just_capture_screenshot_query_targets_async (
    JustCapturePortal  *portal,
    GCancellable       *cancellable,
    GAsyncReadyCallback callback,
    gpointer            user_data);

JustCaptureScreenshotTarget
just_capture_screenshot_query_targets_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

/* Request a screenshot.
 *
 * @target:       target value (0 = let portal decide, or one of the bitmask values)
 * @interactive:  TRUE to show the portal selection UI
 * @parent_window: parent window identifier (can be "" or NULL)
 */
void just_capture_screenshot_request_async (
    JustCapturePortal           *portal,
    JustCaptureScreenshotTarget  target,
    gboolean                     interactive,
    const gchar                 *parent_window,
    GCancellable                *cancellable,
    GAsyncReadyCallback          callback,
    gpointer                     user_data);

JustCaptureScreenshotResult *
just_capture_screenshot_request_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

G_END_DECLS

#endif /* JUSTCAPTURE_SCREENSHOT_PORTAL_H */
