/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/errors.h"
#include <gio/gio.h>

static void
test_quark (void)
{
  GQuark q = just_capture_error_quark ();
  g_assert_cmpuint (q, >, 0);
  g_assert_cmpstr (g_quark_to_string (q), ==, "just-capture-error-quark");
}

static void
test_error_codes (void)
{
  g_autoptr(GError) err = NULL;

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED, "cancelled");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_NOT_SUPPORTED, "not supported");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_NOT_SUPPORTED));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PERMISSION_DENIED, "denied");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PERMISSION_DENIED));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE, "unavailable");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL, "protocol");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_IO, "io");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_IO));
  g_clear_error (&err);

  err = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED, "failed");
  g_assert (g_error_matches (err, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED));
  g_clear_error (&err);
}

static void
test_from_dbus_cancelled (void)
{
  g_autoptr(GError) dbus_err = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
  g_autoptr(GError) out = NULL;
  gboolean ret = just_capture_error_from_dbus (dbus_err, &out);
  g_assert_true (ret);
  g_assert (g_error_matches (out, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED));
}

static void
test_from_dbus_permission (void)
{
  g_autoptr(GError) dbus_err = g_error_new (G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED, "Access denied");
  g_autoptr(GError) out = NULL;
  gboolean ret = just_capture_error_from_dbus (dbus_err, &out);
  g_assert_true (ret);
  g_assert (g_error_matches (out, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PERMISSION_DENIED));
}

static void
test_from_dbus_not_supported (void)
{
  g_autoptr(GError) dbus_err = g_error_new (G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "Not supported");
  g_autoptr(GError) out = NULL;
  gboolean ret = just_capture_error_from_dbus (dbus_err, &out);
  g_assert_true (ret);
  g_assert (g_error_matches (out, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_NOT_SUPPORTED));
}

static void
test_from_dbus_unavailable (void)
{
  g_autoptr(GError) dbus_err = g_error_new (G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN, "No service");
  g_autoptr(GError) out = NULL;
  gboolean ret = just_capture_error_from_dbus (dbus_err, &out);
  g_assert_true (ret);
  g_assert (g_error_matches (out, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE));
}

static void
test_from_dbus_null (void)
{
  g_autoptr(GError) out = NULL;
  gboolean ret = just_capture_error_from_dbus (NULL, &out);
  g_assert_false (ret);
  g_assert_null (out);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/errors/quark", test_quark);
  g_test_add_func ("/errors/codes", test_error_codes);
  g_test_add_func ("/errors/from_dbus/cancelled", test_from_dbus_cancelled);
  g_test_add_func ("/errors/from_dbus/permission", test_from_dbus_permission);
  g_test_add_func ("/errors/from_dbus/not_supported", test_from_dbus_not_supported);
  g_test_add_func ("/errors/from_dbus/unavailable", test_from_dbus_unavailable);
  g_test_add_func ("/errors/from_dbus/null", test_from_dbus_null);
  return g_test_run ();
}
