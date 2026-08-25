/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_CAPABILITIES_H
#define JUSTCAPTURE_CAPABILITIES_H

#include <gio/gio.h>
#include "justcapture/portal-client.h"
#include "justcapture/types.h"
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Query all capabilities asynchronously.
 * Results are cached for 30 seconds. */
void just_capture_capabilities_query_async (
    JustCapturePortal  *portal,
    GCancellable       *cancellable,
    GAsyncReadyCallback callback,
    gpointer            user_data);

JustCaptureCapabilities *
just_capture_capabilities_query_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

G_END_DECLS

#endif /* JUSTCAPTURE_CAPABILITIES_H */
