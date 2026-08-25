/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_ERRORS_H
#define JUSTCAPTURE_ERRORS_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * JustCaptureError:
 * @JUST_CAPTURE_ERROR_CANCELLED: User cancelled the portal dialog (not a real error)
 * @JUST_CAPTURE_ERROR_NOT_SUPPORTED: Desktop environment does not support the requested feature
 * @JUST_CAPTURE_ERROR_PERMISSION_DENIED: Access denied by the portal
 * @JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE: Portal service is not available
 * @JUST_CAPTURE_ERROR_PROTOCOL: D-Bus protocol error or malformed response
 * @JUST_CAPTURE_ERROR_IO: File I/O error
 * @JUST_CAPTURE_ERROR_FAILED: Generic failure
 */
#define JUST_CAPTURE_ERROR (just_capture_error_quark ())
GQuark just_capture_error_quark (void);

typedef enum {
  JUST_CAPTURE_ERROR_CANCELLED,
  JUST_CAPTURE_ERROR_NOT_SUPPORTED,
  JUST_CAPTURE_ERROR_PERMISSION_DENIED,
  JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE,
  JUST_CAPTURE_ERROR_PROTOCOL,
  JUST_CAPTURE_ERROR_IO,
  JUST_CAPTURE_ERROR_FAILED,
} JustCaptureError;

/* Map common D-Bus / GIO errors to JustCaptureError domain */
gboolean just_capture_error_from_dbus (GError  *dbus_error,
                                       GError **out_error);

G_END_DECLS

#endif /* JUSTCAPTURE_ERRORS_H */
