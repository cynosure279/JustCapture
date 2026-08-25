/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/errors.h"
#include <gio/gio.h>

G_DEFINE_QUARK (just-capture-error-quark, just_capture_error)

/**
 * just_capture_error_from_dbus:
 * @dbus_error: a GError from a D-Bus / GIO operation
 * @out_error: (out): return location for the mapped error
 *
 * Maps common D-Bus and GIO errors onto the #JustCaptureError domain.
 *
 * Returns: %TRUE if @out_error was set
 */
gboolean
just_capture_error_from_dbus (GError  *dbus_error,
                              GError **out_error)
{
  g_return_val_if_fail (out_error != NULL, FALSE);

  if (dbus_error == NULL)
    return FALSE;

  const gchar *msg = dbus_error->message != NULL
                     ? dbus_error->message
                     : "Unknown D-Bus error";
  JustCaptureError code;

  if (g_error_matches (dbus_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    code = JUST_CAPTURE_ERROR_CANCELLED;
  else if (g_error_matches (dbus_error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
    code = JUST_CAPTURE_ERROR_PERMISSION_DENIED;
  else if (g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD))
    code = JUST_CAPTURE_ERROR_NOT_SUPPORTED;
  else if (g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_NO_SERVER))
    code = JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE;
  else if (g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT) ||
           g_error_matches (dbus_error, G_DBUS_ERROR, G_DBUS_ERROR_TIMEOUT))
    code = JUST_CAPTURE_ERROR_PROTOCOL;
  else
    code = JUST_CAPTURE_ERROR_FAILED;

  g_set_error (out_error, JUST_CAPTURE_ERROR, code, "%s", msg);
  return TRUE;
}