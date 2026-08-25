/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/capabilities.h"

typedef enum {
  PROP_AVAILABLE_TARGETS,
  PROP_SCREENSHOT_VERSION,
  PROP_AVAILABLE_SOURCE_TYPES,
  PROP_AVAILABLE_CURSOR_MODES,
  PROP_SCREENCAST_VERSION,
  PROP_COUNT
} CapProperty;

static const struct {
  const gchar *interface;
  const gchar *property;
} prop_info[PROP_COUNT] = {
  [PROP_AVAILABLE_TARGETS]      = { "org.freedesktop.portal.Screenshot", "AvailableTargets" },
  [PROP_SCREENSHOT_VERSION]     = { "org.freedesktop.portal.Screenshot", "version" },
  [PROP_AVAILABLE_SOURCE_TYPES] = { "org.freedesktop.portal.ScreenCast", "AvailableSourceTypes" },
  [PROP_AVAILABLE_CURSOR_MODES] = { "org.freedesktop.portal.ScreenCast", "AvailableCursorModes" },
  [PROP_SCREENCAST_VERSION]     = { "org.freedesktop.portal.ScreenCast", "version" },
};

typedef struct {
  JustCaptureCapabilities *caps;
  GTask                   *task;     /* master task to complete */
  gint                     pending;  /* remaining calls */
  GMainContext            *context;  /* correct context for master task */
} CapsQuery;

static void
caps_query_free (CapsQuery *q)
{
  if (q == NULL) return;
  g_clear_pointer (&q->caps, just_capture_capabilities_free);
  g_clear_object (&q->task);
  g_clear_pointer (&q->context, g_main_context_unref);
  g_free (q);
}

static void
store_property (JustCaptureCapabilities *caps, CapProperty p, guint val)
{
  switch (p)
    {
    case PROP_AVAILABLE_TARGETS:    caps->screenshot_targets = (JustCaptureScreenshotTarget) val; break;
    case PROP_SCREENSHOT_VERSION:   caps->screenshot_portal_version = val; break;
    case PROP_AVAILABLE_SOURCE_TYPES: caps->screencast_source_types = val; break;
    case PROP_AVAILABLE_CURSOR_MODES: caps->screencast_cursor_modes = val; break;
    case PROP_SCREENCAST_VERSION:   caps->screencast_portal_version = val; break;
    default: break;
    }
}

static void
on_property_read (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GTask *prop_task = G_TASK (user_data);
  CapsQuery *q = g_task_get_source_object (prop_task);
  CapProperty p = (CapProperty) GPOINTER_TO_SIZE (g_task_get_task_data (prop_task));

  g_autoptr(GVariant) value = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, NULL);
  if (value != NULL)
    {
      GVariant *v = g_variant_get_child_value (value, 0);
      if (g_variant_is_of_type (v, G_VARIANT_TYPE_UINT32))
        store_property (q->caps, p, g_variant_get_uint32 (v));
      g_variant_unref (v);
    }

  g_object_unref (prop_task);
  q->pending--;

  if (q->pending <= 0)
    {
      q->caps->portal_available = TRUE;
      g_task_return_pointer (q->task, q->caps, NULL);
      q->caps = NULL;
      caps_query_free (q);
    }
}

void
just_capture_capabilities_query_async (JustCapturePortal  *portal,
                                       GCancellable       *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer            user_data)
{
  g_return_if_fail (portal != NULL);

  GTask *master = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (master, just_capture_capabilities_query_async);

  GDBusConnection *conn = just_capture_portal_get_connection (portal);
  if (conn == NULL || !just_capture_portal_is_available (portal))
    {
      JustCaptureCapabilities *caps = g_new0 (JustCaptureCapabilities, 1);
      caps->portal_available = FALSE;
      g_task_return_pointer (master, caps, NULL);
      g_object_unref (master);
      return;
    }

  CapsQuery *q = g_new0 (CapsQuery, 1);
  q->caps = g_new0 (JustCaptureCapabilities, 1);
  q->task = master;
  q->pending = PROP_COUNT;
  q->context = g_main_context_ref (g_main_context_get_thread_default ());

  for (gint i = 0; i < PROP_COUNT; i++)
    {
      GTask *prop_task = g_task_new (q, NULL, NULL, NULL);
      g_task_set_task_data (prop_task, GSIZE_TO_POINTER (i), NULL);

      g_dbus_connection_call (conn,
                              "org.freedesktop.portal.Desktop",
                              "/org/freedesktop/portal/desktop",
                              "org.freedesktop.DBus.Properties",
                              "Get",
                              g_variant_new ("(ss)", prop_info[i].interface, prop_info[i].property),
                              G_VARIANT_TYPE ("(v)"),
                              G_DBUS_CALL_FLAGS_NONE, -1,
                              cancellable,
                              on_property_read,
                              prop_task);
    }
}

JustCaptureCapabilities *
just_capture_capabilities_query_finish (JustCapturePortal  *portal G_GNUC_UNUSED,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}