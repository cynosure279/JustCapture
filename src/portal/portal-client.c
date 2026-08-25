/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * Manages the session-bus connection to the XDG Desktop Portal and
 * caches D-Bus proxies for the portal interfaces used by JustCapture.
 */

#include "justcapture/portal-client.h"

#define PORTAL_NAME       "org.freedesktop.portal.Desktop"
#define PORTAL_PATH       "/org/freedesktop/portal/desktop"

struct _JustCapturePortal {
  GDBusConnection *connection;
  GHashTable      *proxies;      /* interface name -> GDBusProxy */
  guint            name_watch_id;
  gboolean         portal_available;
  gboolean         watch_started;
};

static JustCapturePortal *portal_singleton = NULL;

/**
 * just_capture_portal_get_default:
 *
 * Returns the process-wide portal client singleton.
 */
JustCapturePortal *
just_capture_portal_get_default (void)
{
  if (portal_singleton == NULL)
    {
      portal_singleton = g_new0 (JustCapturePortal, 1);
      portal_singleton->proxies = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                         g_free, g_object_unref);
      portal_singleton->name_watch_id = 0;
      portal_singleton->portal_available = FALSE;
      portal_singleton->watch_started = FALSE;
    }

  return portal_singleton;
}

/**
 * ensure_connection:
 *
 * Lazily resolves the session bus. The session bus connection is a
 * local, cached singleton in GLib so this is effectively free after
 * the first call.
 */
static gboolean
ensure_connection (JustCapturePortal *portal,
                   GError           **error)
{
  if (portal->connection != NULL)
    return TRUE;

  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  if (connection == NULL)
    return FALSE;

  portal->connection = connection;   /* transfer: owns a ref */
  return TRUE;
}

static void
on_name_appeared (GDBusConnection *connection G_GNUC_UNUSED,
                  const gchar     *name G_GNUC_UNUSED,
                  const gchar     *name_owner G_GNUC_UNUSED,
                  gpointer         user_data)
{
  JustCapturePortal *portal = user_data;
  portal->portal_available = TRUE;
}

static void
on_name_vanished (GDBusConnection *connection G_GNUC_UNUSED,
                  const gchar     *name G_GNUC_UNUSED,
                  gpointer         user_data)
{
  JustCapturePortal *portal = user_data;
  portal->portal_available = FALSE;
}

static void
ensure_name_watch (JustCapturePortal *portal)
{
  if (portal->watch_started)
    return;

  portal->watch_started = TRUE;
  portal->name_watch_id = g_bus_watch_name_on_connection (portal->connection,
                                                          PORTAL_NAME,
                                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                          on_name_appeared,
                                                          on_name_vanished,
                                                          portal, NULL);
}

static void
name_has_owner_cb (GObject      *source_object G_GNUC_UNUSED,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  JustCapturePortal *portal = g_task_get_source_object (task);
  g_autoptr(GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
  if (reply == NULL)
    {
      g_task_return_boolean (task, FALSE);
      g_object_unref (task);
      return;
    }

  gboolean available = FALSE;
  g_variant_get (reply, "(b)", &available);
  portal->portal_available = available;

  g_task_return_boolean (task, available);
  g_object_unref (task);
}

/**
 * just_capture_portal_check_available_async:
 *
 * Asynchronously checks whether the portal service is available.
 * The finish function returns %TRUE if the portal name has an owner.
 */
void
just_capture_portal_check_available_async (JustCapturePortal  *portal,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data)
{
  g_return_if_fail (portal != NULL);

  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, just_capture_portal_check_available_async);

  GError *error = NULL;
  if (!ensure_connection (portal, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  ensure_name_watch (portal);

  g_dbus_connection_call (portal->connection,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "NameHasOwner",
                          g_variant_new ("(s)", PORTAL_NAME),
                          G_VARIANT_TYPE ("(b)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          name_has_owner_cb,
                          task);
}

/**
 * just_capture_portal_check_available_finish:
 *
 * Returns %TRUE if the portal is available.
 */
gboolean
just_capture_portal_check_available_finish (JustCapturePortal  *portal G_GNUC_UNUSED,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * just_capture_portal_get_connection:
 *
 * Returns the session bus connection, or %NULL if it could not be
 * established.
 */
GDBusConnection *
just_capture_portal_get_connection (JustCapturePortal *portal)
{
  g_return_val_if_fail (portal != NULL, NULL);

  if (!ensure_connection (portal, NULL))
    return NULL;

  return portal->connection;
}

/**
 * just_capture_portal_get_proxy:
 *
 * Returns a cached D-Bus proxy for @interface_name bound to the portal
 * service, creating it if necessary.
 */
GDBusProxy *
just_capture_portal_get_proxy (JustCapturePortal *portal,
                               const gchar       *interface_name)
{
  g_return_val_if_fail (portal != NULL, NULL);
  g_return_val_if_fail (interface_name != NULL, NULL);

  GDBusProxy *proxy = g_hash_table_lookup (portal->proxies, interface_name);
  if (proxy != NULL)
    return g_object_ref (proxy);  /* caller owns the returned reference */

  if (!ensure_connection (portal, NULL))
    return NULL;

  GError *error = NULL;
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                         NULL,  /* GDBusInterfaceInfo - NULL for auto-introspect */
                                         PORTAL_NAME,
                                         PORTAL_PATH,
                                         interface_name,
                                         NULL,  /* cancellable */
                                         &error);
  if (proxy == NULL)
    {
      g_warning ("justcapture: failed to create proxy for %s: %s",
                 interface_name, error->message);
      g_error_free (error);
      return NULL;
    }

  g_hash_table_insert (portal->proxies, g_strdup (interface_name), proxy);
  return g_object_ref (proxy);  /* caller owns the returned reference */
}

/**
 * just_capture_portal_is_available:
 *
 * Synchronous availability check based on the name-owner watch.
 */
gboolean
just_capture_portal_is_available (JustCapturePortal *portal)
{
  g_return_val_if_fail (portal != NULL, FALSE);

  if (!ensure_connection (portal, NULL))
    return FALSE;

  ensure_name_watch (portal);
  return portal->portal_available;
}