/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * Screenshot Portal wrapper.
 *
 * Provides asynchronous query of available targets and screenshot
 * capture via the XDG Desktop Portal Screenshot interface.
 */

#include "justcapture/screenshot-portal.h"
#include "justcapture/portal-request.h"

/* ─────────────────────────────────────────
 *  Query targets
 * ───────────────────────────────────────── */

typedef struct {
  JustCaptureScreenshotTarget targets;
  GTask *task;
  guint  pending;
} QueryTargetsData;

static void
query_targets_property_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  QueryTargetsData *data = user_data;
  GError *error = NULL;

  g_autoptr(GVariant) value = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
  if (value != NULL)
    {
      GVariant *v = g_variant_get_child_value (value, 0);
      if (g_variant_is_of_type (v, G_VARIANT_TYPE_UINT32))
        data->targets |= g_variant_get_uint32 (v);
      g_variant_unref (v);
    }
  else
    {
      g_debug ("justcapture: failed to read AvailableTargets: %s", error->message);
      g_error_free (error);
    }

  data->pending--;
  if (data->pending == 0)
    {
      JustCaptureScreenshotResult *result = g_new0 (JustCaptureScreenshotResult, 1);
      result->actual_target = data->targets;
      g_task_return_pointer (data->task, GINT_TO_POINTER (data->targets), NULL);
      g_free (data);
    }
}

/**
 * just_capture_screenshot_query_targets_async:
 *
 * Reads the AvailableTargets property of the Screenshot portal.
 * The finish function returns a bitmask of #JustCaptureScreenshotTarget.
 */
void
just_capture_screenshot_query_targets_async (JustCapturePortal  *portal,
                                             GCancellable       *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer            user_data)
{
  g_return_if_fail (portal != NULL);

  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, just_capture_screenshot_query_targets_async);

  GDBusConnection *conn = just_capture_portal_get_connection (portal);
  if (conn == NULL)
    {
      g_task_return_new_error (task, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE,
                               "Session bus not available");
      g_object_unref (task);
      return;
    }

  QueryTargetsData *data = g_new0 (QueryTargetsData, 1);
  data->task = task;
  data->pending = 1;

  /* Read AvailableTargets via Properties.Get */
  g_dbus_connection_call (conn,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          g_variant_new ("(ss)",
                                         "org.freedesktop.portal.Screenshot",
                                         "AvailableTargets"),
                          G_VARIANT_TYPE ("(v)"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          cancellable,
                          query_targets_property_cb,
                          data);
}

JustCaptureScreenshotTarget
just_capture_screenshot_query_targets_finish (JustCapturePortal  *portal G_GNUC_UNUSED,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), 0);

  gintptr val = (gintptr) g_task_propagate_pointer (G_TASK (result), error);
  return (JustCaptureScreenshotTarget) val;
}

/* ─────────────────────────────────────────
 *  Screenshot request
 * ───────────────────────────────────────── */

/**
 * just_capture_screenshot_request_async:
 *
 * Initiates a screenshot via the Portal.
 * The finish function returns a #JustCaptureScreenshotResult.
 */
void
just_capture_screenshot_request_async (JustCapturePortal           *portal,
                                       JustCaptureScreenshotTarget  target,
                                       gboolean                     interactive,
                                       const gchar                 *parent_window,
                                       GCancellable                *cancellable,
                                       GAsyncReadyCallback          callback,
                                       gpointer                     user_data)
{
  g_return_if_fail (portal != NULL);

  /* Build options dict */
  GVariant *opts = g_variant_ref_sink (just_capture_portal_build_options (portal));
  GVariantDict dict;
  g_variant_dict_init (&dict, opts);
  g_variant_dict_insert (&dict, "modal", "b", TRUE);
  g_variant_dict_insert (&dict, "interactive", "b", interactive ? TRUE : FALSE);
  if (target != 0)
    g_variant_dict_insert (&dict, "target", "u", (guint) target);
  GVariant *options = g_variant_ref_sink (g_variant_dict_end (&dict));
  g_variant_unref (opts);

  GVariant *params = g_variant_new ("(s@a{sv})",
                                    parent_window ? parent_window : "",
                                    options);

  just_capture_portal_request_call (portal,
                                    "org.freedesktop.portal.Screenshot",
                                    "Screenshot",
                                    params,
                                    120000,
                                    cancellable,
                                    callback,
                                    user_data);
}

JustCaptureScreenshotResult *
just_capture_screenshot_request_finish (JustCapturePortal  *portal G_GNUC_UNUSED,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  GVariant *results = just_capture_portal_request_call_finish (result, error);
  if (results == NULL)
    return NULL;

  JustCaptureScreenshotResult *sr = g_new0 (JustCaptureScreenshotResult, 1);

  const gchar *uri = NULL;
  if (g_variant_lookup (results, "uri", "&s", &uri))
    sr->uri = g_strdup (uri);
  g_variant_unref (results);

  return sr;
}