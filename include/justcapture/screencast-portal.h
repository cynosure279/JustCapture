/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_SCREENCAST_PORTAL_H
#define JUSTCAPTURE_SCREENCAST_PORTAL_H

#include <gio/gio.h>
#include "justcapture/portal-client.h"
#include "justcapture/types.h"
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Create a complete ScreenCast session (CreateSession → SelectSources → Start → OpenPipeWireRemote). */
void just_capture_screencast_session_create_async (
    JustCapturePortal    *portal,
    guint                 source_types,      /* bitmask of JustCaptureSourceType */
    JustCaptureCursorMode cursor_mode,       /* cursor mode */
    gboolean              allow_multiple,    /* allow multiple sources */
    const gchar          *restore_token,     /* previous restore token, or NULL */
    GCancellable         *cancellable,
    GAsyncReadyCallback   callback,
    gpointer              user_data);

JustCaptureScreenCastSession *
just_capture_screencast_session_create_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

/* Close and clean up a ScreenCast session.
 * The session struct is freed after completion. */
void just_capture_screencast_session_close_async (
    JustCapturePortal            *portal,
    JustCaptureScreenCastSession *session,
    GCancellable                 *cancellable,
    GAsyncReadyCallback           callback,
    gpointer                      user_data);

gboolean just_capture_screencast_session_close_finish (
    JustCapturePortal  *portal,
    GAsyncResult       *result,
    GError            **error);

G_END_DECLS

#endif /* JUSTCAPTURE_SCREENCAST_PORTAL_H */
