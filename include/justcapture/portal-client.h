/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_PORTAL_CLIENT_H
#define JUSTCAPTURE_PORTAL_CLIENT_H

#include <gio/gio.h>
#include "justcapture/errors.h"

G_BEGIN_DECLS

typedef struct _JustCapturePortal JustCapturePortal;

/* Get the singleton portal client instance. */
JustCapturePortal *just_capture_portal_get_default (void);

/* Check portal availability asynchronously.
 * The callback receives a gboolean TRUE if the portal is available
 * (name owner exists and responds to property queries), FALSE otherwise. */
void just_capture_portal_check_available_async (
    JustCapturePortal  *portal,
    GCancellable       *cancellable,
    GAsyncReadyCallback callback,
    gpointer            user_data);

gboolean just_capture_portal_check_available_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

/* Get the session bus connection (lazy initialised). */
GDBusConnection *just_capture_portal_get_connection (JustCapturePortal *portal);

/* Get a cached D-Bus proxy for the given interface on the portal name.
 * The proxy is created lazily and cached. May return NULL on error. */
GDBusProxy *just_capture_portal_get_proxy (JustCapturePortal    *portal,
                                           const gchar          *interface_name);

/* Quick synchronous availability check (no async). */
gboolean just_capture_portal_is_available (JustCapturePortal *portal);

G_END_DECLS

#endif /* JUSTCAPTURE_PORTAL_CLIENT_H */
