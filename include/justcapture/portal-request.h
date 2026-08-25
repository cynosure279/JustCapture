/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_PORTAL_REQUEST_H
#define JUSTCAPTURE_PORTAL_REQUEST_H

#include <gio/gio.h>
#include "justcapture/portal-client.h"
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Build an options dict with a fresh handle_token inside.
 * Callers add extra method-specific keys via GVariantDict. */
GVariant *just_capture_portal_build_options (JustCapturePortal *portal);

/* Send a portal request and wait for the Response signal.
 *
 * @interface_name: D-Bus interface, e.g. "org.freedesktop.portal.Screenshot"
 * @method_name:    method name, e.g. "Screenshot"
 * @parameters:     full GVariant tuple for the method call
 * @timeout_msec:   timeout in ms (-1 for default 120s)
 * @cancellable:    GCancellable
 * @callback:       GAsyncReadyCallback
 * @user_data:      user data
 *
 * The finish function returns the Response signal's results dict (a{sv}).
 * Callers must unref the returned GVariant after use.
 */
void just_capture_portal_request_call (
    JustCapturePortal  *portal,
    const gchar        *interface_name,
    const gchar        *method_name,
    GVariant           *parameters,
    gint                timeout_msec,
    GCancellable       *cancellable,
    GAsyncReadyCallback callback,
    gpointer            user_data);

/* Finish a portal request call.
 * Returns the results dict (a{sv}) from the Response signal, or NULL on error. */
GVariant *just_capture_portal_request_call_finish (
    GAsyncResult *result,
    GError      **error);

G_END_DECLS

#endif /* JUSTCAPTURE_PORTAL_REQUEST_H */
