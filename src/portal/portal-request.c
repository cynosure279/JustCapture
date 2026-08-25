/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * Generic portal Request lifecycle helper.
 *
 * Every portal method that returns a request object path follows the same
 * pattern: call method → get {request, ...} → subscribe to Response signal
 * on the request path → wait for Response(u code, a{sv} results).
 *
 * This module encapsulates that pattern so callers can focus on their
 * method-specific logic.
 */

#include "justcapture/portal-request.h"
#include <unistd.h>

#define DEFAULT_TIMEOUT_MS 120000  /* 2 minutes for interactive dialogs */

typedef struct {
  gchar         *request_path;   /* object path of the portal Request */
  GDBusConnection *connection;
  guint          response_sub_id;/* Response signal subscription id */
  GCancellable  *cancellable;
  gulong         cancel_id;      /* signal handler id on cancellable */
  GTask         *task;
  gint           timeout_id;     /* g_timeout_add source id */
  gboolean       completed;
  GVariant      *method_reply;   /* a{sv} from the method call */
  GVariant      *results;        /* merged a{sv} result from Response */
  gboolean       response_arrived; /* TRUE if Response signal already fired */
  guint          response_code;    /* Response code from the signal */
  GVariant      *response_results; /* Response results (owned by us) */
} PortalRequest;

/* Cache of pending Response signals that arrived before on_method_call.
 * Maps request_path -> PortalRequest* (the request that should receive it). */
static GHashTable *pending_responses = NULL;

/* ─────────────────────────────────────────
 *  Forward declarations
 * ───────────────────────────────────────── */

static void complete_request (PortalRequest *req, gboolean success, GVariant *results, GError *error);
static void maybe_cleanup (PortalRequest *req);
static void process_response (PortalRequest *req);

/* ─────────────────────────────────────────
 *  just_capture_portal_build_options
 * ───────────────────────────────────────── */

/**
 * just_capture_portal_build_options:
 * @portal: a #JustCapturePortal
 *
 * Returns a new options dict (a{sv}) with a fresh handle_token.
 * The caller should add method-specific keys via g_variant_dict_insert()
 * and pass the final dict to the request method.
 *
 * Returns: (transfer floating) a new #GVariant of type a{sv}
 */
GVariant *
just_capture_portal_build_options (JustCapturePortal *portal G_GNUC_UNUSED)
{
  g_autofree gchar *token = g_strdup_printf ("justcapture%u%u",
                                             (guint) getpid (),
                                             (guint) g_random_int ());
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (builder, "{sv}", "handle_token", g_variant_new_string (token));
  return g_variant_builder_end (builder);
}

/* ─────────────────────────────────────────
 *  Internal helpers
 * ───────────────────────────────────────── */

static void
complete_request (PortalRequest *req,
                  gboolean       success,
                  GVariant      *results,
                  GError        *error)
{
  if (req->completed)
    return;

  req->completed = TRUE;
  req->results = results; /* may be NULL */

  if (success)
    g_task_return_pointer (req->task, results, (GDestroyNotify) g_variant_unref);
  else
    g_task_return_error (req->task, error);

  maybe_cleanup (req);
}

static void
maybe_cleanup (PortalRequest *req)
{
  if (!req->completed)
    return;

  /* task is now owned by the caller via g_task_propagate_pointer;
   * we only clean up non-task resources */
  req->task = NULL;

  if (req->response_sub_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (req->connection, req->response_sub_id);
      req->response_sub_id = 0;
    }

  if (req->timeout_id != 0)
    {
      g_source_remove (req->timeout_id);
      req->timeout_id = 0;
    }

  if (req->cancel_id != 0)
    {
      g_cancellable_disconnect (req->cancellable, req->cancel_id);
      req->cancel_id = 0;
    }

  g_clear_pointer (&req->request_path, g_free);
  g_clear_pointer (&req->method_reply, g_variant_unref);
  g_clear_pointer (&req->response_results, g_variant_unref);
  g_free (req);
}

/* ─────────────────────────────────────────
 *  Cancel handler
 * ───────────────────────────────────────── */

static void
on_cancelled (GCancellable *cancellable G_GNUC_UNUSED,
              gpointer      user_data)
{
  PortalRequest *req = user_data;

  if (req->completed)
    return;

  /* Tell the portal we are done */
  if (req->connection != NULL && req->request_path != NULL)
    {
      GVariantBuilder *cb = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (cb, "{sv}", "handle_token", g_variant_new_string ("cancel"));
      GVariant *close_opts = g_variant_builder_end (cb);
      g_variant_builder_unref (cb);
      g_dbus_connection_call (req->connection,
                              "org.freedesktop.portal.Desktop",
                              req->request_path,
                              "org.freedesktop.portal.Request",
                              "Close",
                              g_variant_new ("(@a{sv})", close_opts),
                              NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL, NULL);
    }

  complete_request (req, FALSE, NULL,
                    g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED,
                                 "Portal request was cancelled by the user"));
}

/* ─────────────────────────────────────────
 *  Timeout handler
 * ───────────────────────────────────────── */

static gboolean
on_timeout (gpointer user_data)
{
  PortalRequest *req = user_data;

  if (req->completed)
    return G_SOURCE_REMOVE;

  req->timeout_id = 0;  /* already removed by g_source_set_name */

  complete_request (req, FALSE, NULL,
                    g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED,
                                 "Portal request timed out"));
  return G_SOURCE_REMOVE;
}

/* ─────────────────────────────────────────
 *  Response signal handler
 * ───────────────────────────────────────── */

static void
on_response (GDBusConnection *connection G_GNUC_UNUSED,
             const gchar     *sender_name G_GNUC_UNUSED,
             const gchar     *object_path,
             const gchar     *interface_name G_GNUC_UNUSED,
             const gchar     *signal_name G_GNUC_UNUSED,
             GVariant        *parameters,
             gpointer         user_data)
{
  PortalRequest *req = user_data;

  if (req->completed)
    return;

  /* Only handle responses for our request path (NULL path = not yet known, accept all) */
  if (req->request_path != NULL && g_strcmp0 (object_path, req->request_path) != 0)
    return;

  guint code;
  GVariant *results = NULL;

  g_variant_get (parameters, "(u@a{sv})", &code, &results);

  /* Save the response data */
  req->response_arrived = TRUE;
  req->response_code = code;
  req->response_results = results;  /* takes ownership */

  /* If on_method_call hasn't run yet, wait for it to process the response. */
  if (req->method_reply == NULL && !req->completed)
    return;  /* on_method_call will call process_response */

  process_response (req);
}

/* Process a cached or immediate response */
static void
process_response (PortalRequest *req)
{
  if (req->completed || !req->response_arrived)
    return;

  guint code = req->response_code;
  GVariant *results = req->response_results;

  switch (code)
    {
    case 0: /* SUCCESS */
      {
        GVariantDict merged;
        if (req->method_reply != NULL && g_variant_is_of_type (req->method_reply, G_VARIANT_TYPE ("a{sv}")))
          g_variant_dict_init (&merged, req->method_reply);
        else
          g_variant_dict_init (&merged, NULL);
        if (results != NULL)
          {
            GVariantIter iter;
            g_variant_iter_init (&iter, results);
            const gchar *key;
            GVariant *val;
            while (g_variant_iter_next (&iter, "{&sv}", &key, &val))
              g_variant_dict_insert_value (&merged, key, val);
          }
        GVariant *merged_results = g_variant_ref_sink (g_variant_dict_end (&merged));
        complete_request (req, TRUE, merged_results, NULL);
        break;
      }

    case 1: /* CANCELLED */
      complete_request (req, FALSE, NULL,
                        g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_CANCELLED,
                                     "User cancelled the portal dialog"));
      break;

    case 2: /* OTHER */
      {
        const gchar *err_msg = "Unknown portal error";
        if (results != NULL)
          g_variant_lookup (results, "error", "&s", &err_msg);
        JustCaptureError error_code = JUST_CAPTURE_ERROR_FAILED;
        if (g_strstr_len (err_msg, -1, "denied") ||
            g_strstr_len (err_msg, -1, "Denied") ||
            g_strstr_len (err_msg, -1, "permission") ||
            g_strstr_len (err_msg, -1, "Permission"))
          error_code = JUST_CAPTURE_ERROR_PERMISSION_DENIED;
        complete_request (req, FALSE, NULL,
                          g_error_new (JUST_CAPTURE_ERROR, error_code,
                                       "%s", err_msg));
        break;
      }

    default:
      complete_request (req, FALSE, NULL,
                        g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL,
                                     "Unexpected portal Response code %u", code));
      break;
    }
}

/* ─────────────────────────────────────────
 *  Method call reply handler
 * ───────────────────────────────────────── */

static void
on_method_call (GObject      *source_object G_GNUC_UNUSED,
                GAsyncResult *res,
                gpointer      user_data)
{
  PortalRequest *req = user_data;
  GError *error = NULL;

  g_autoptr(GVariant) reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
  if (reply == NULL)
    {
      /* Map D-Bus error to JustCaptureError */
      g_autoptr(GError) mapped = NULL;
      if (error != NULL && !just_capture_error_from_dbus (error, &mapped))
        mapped = g_error_copy (error);
      else if (error == NULL)
        mapped = g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED,
                              "Portal method call failed with no error");
      complete_request (req, FALSE, NULL, g_steal_pointer (&mapped));
      g_clear_error (&error);
      return;
    }

  /* Handle both (a{sv}) and (o) reply types */
  const gchar *request_path = NULL;

  if (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(a{sv})")))
    {
      /* Reply is a dict — extract request path from it */
      GVariant *dict = NULL;
      g_variant_get (reply, "(@a{sv})", &dict);
      req->method_reply = g_variant_ref_sink (dict);
      g_variant_lookup (req->method_reply, "request", "&o", &request_path);
    }
  else if (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(o)")))
    {
      /* Reply is just an object path — create empty dict for method_reply */
      g_variant_get (reply, "(&o)", &request_path);
      req->method_reply = g_variant_ref_sink (g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0));
    }
  else
    {
      complete_request (req, FALSE, NULL,
                        g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL,
                                     "Unexpected portal reply type '%s'",
                                     g_variant_get_type_string (reply)));
      return;
    }

  if (request_path == NULL)
    {
      complete_request (req, FALSE, NULL,
                        g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL,
                                     "Method reply did not contain a request path"));
      return;
    }

  g_message ("portal-request: on_method_call got request_path=%s (predicted=%s)", request_path, req->request_path ? req->request_path : "(none)");
  /* If we pre-subscribed with a predicted path, verify it matches */
  if (req->request_path == NULL)
    {
      req->request_path = g_strdup (request_path);
      req->response_sub_id = g_dbus_connection_signal_subscribe (req->connection,
                                                                 NULL,
                                                                 "org.freedesktop.portal.Request",
                                                                 "Response",
                                                                 req->request_path,
                                                                 NULL,
                                                                 G_DBUS_SIGNAL_FLAGS_NONE,
                                                                 on_response,
                                                                 req,
                                                                 NULL);
    }

  g_message ("portal-request: on_method_call done, response_arrived=%d", req->response_arrived);

  /* If the Response already arrived (race condition), process it now */
  if (req->response_arrived)
    process_response (req);

  /* Arm timeout */
  if (req->timeout_id == 0)
    req->timeout_id = g_timeout_add_seconds (120, on_timeout, req);
}

/* ─────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────── */


/* Get the unique bus name of this connection (e.g. ":1.231") */
static gchar *
get_unique_bus_name (GDBusConnection *conn)
{
  GError *error = NULL;
  GVariant *reply = g_dbus_connection_call_sync (conn,
      "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "GetNameOwner",
      g_variant_new ("(s)", g_dbus_connection_get_unique_name (conn)),
      G_VARIANT_TYPE ("(s)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  if (reply)
    {
      g_variant_unref (reply);
      g_error_free (error);
      return g_strdup (g_dbus_connection_get_unique_name (conn));
    }
  g_error_free (error);
  return g_strdup (g_dbus_connection_get_unique_name (conn));
}

/* Extract handle_token from the last a{sv} parameter and predict the
 * request object path. Returns NULL if token not found. */
static gchar *
predict_request_path_from_params (GDBusConnection *conn, GVariant *parameters)
{
  gsize n = g_variant_n_children (parameters);
  if (n == 0)
    return NULL;

  /* The options dict is the LAST child of the parameters tuple */
  GVariant *last = g_variant_get_child_value (parameters, n - 1);
  if (!g_variant_is_of_type (last, G_VARIANT_TYPE ("a{sv}")))
    {
      g_variant_unref (last);
      return NULL;
    }

  const gchar *token = NULL;
  g_variant_lookup (last, "handle_token", "&s", &token);
  g_variant_unref (last);

  if (token == NULL)
    return NULL;

  /* Get our unique bus name and sanitize it for the path */
  const gchar *uniq = g_dbus_connection_get_unique_name (conn);
  if (uniq == NULL)
    return NULL;

  /* The portal converts the unique bus name to a path-safe format:
   * ":1.234" becomes "1_234" (colon removed, dot replaced with _) */
  GString *sender_sanitized = g_string_new (NULL);
  for (const gchar *p = uniq; *p; p++)
    {
      if (*p == ':')
        continue;  /* skip colon */
      if (g_ascii_isalnum (*p))
        g_string_append_c (sender_sanitized, *p);
      else
        g_string_append_c (sender_sanitized, '_');
    }

  gchar *path = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s",
                                  sender_sanitized->str, token);
  g_string_free (sender_sanitized, TRUE);
  return path;
}

/**
 * just_capture_portal_request_call:
 *
 * Sends a portal method call and waits for the Response signal.
 * @parameters is consumed (the caller should not unref it).
 */
void
just_capture_portal_request_call (JustCapturePortal  *portal,
                                  const gchar        *interface_name,
                                  const gchar        *method_name,
                                  GVariant           *parameters,
                                  gint                timeout_msec,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  g_return_if_fail (portal != NULL);
  g_return_if_fail (interface_name != NULL);
  g_return_if_fail (method_name != NULL);
  g_return_if_fail (parameters != NULL);

  PortalRequest *req = g_new0 (PortalRequest, 1);
  req->task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (req->task, just_capture_portal_request_call);
  req->connection = just_capture_portal_get_connection (portal);
  req->cancellable = cancellable;

  if (timeout_msec <= 0)
    timeout_msec = DEFAULT_TIMEOUT_MS;

  /* Connect cancellable */
  if (cancellable != NULL)
    req->cancel_id = g_cancellable_connect (cancellable,
                                            G_CALLBACK (on_cancelled),
                                            req, NULL);

  /* Set timeout (used for the Response wait) */
  if (req->timeout_id == 0)
    req->timeout_id = g_timeout_add (timeout_msec, on_timeout, req);

  /* Predict the request path and subscribe to Response BEFORE the method
   * call to avoid the race condition where xdpw sends the Response before
   * on_method_call can subscribe. */
  gchar *predicted_path = predict_request_path_from_params (req->connection, parameters);
  if (predicted_path != NULL)
    {
      req->request_path = predicted_path;  /* will be verified in on_method_call */
      req->response_sub_id = g_dbus_connection_signal_subscribe (req->connection,
                                                                 NULL,
                                                                 "org.freedesktop.portal.Request",
                                                                 "Response",
                                                                 req->request_path,
                                                                 NULL,
                                                                 G_DBUS_SIGNAL_FLAGS_NONE,
                                                                 on_response,
                                                                 req,
                                                                 NULL);
      g_message ("portal-request: pre-subscribed to %s sub_id=%u", predicted_path, req->response_sub_id);
    }

  /* Make the method call via the connection directly (avoids GDBusProxy
   * introspection issues). g_dbus_connection_call() consumes the floating
   * @parameters reference. */
  g_dbus_connection_call (req->connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          interface_name,
                          method_name,
                          parameters,
                          NULL,  /* reply type — accept any */
                          G_DBUS_CALL_FLAGS_NONE, timeout_msec,
                          cancellable, on_method_call, req);
}

/**
 * just_capture_portal_request_call_finish:
 *
 * Completes the request and returns the merged results dict (a{sv}).
 * The caller owns the returned #GVariant and must unref it.
 */
GVariant *
just_capture_portal_request_call_finish (GAsyncResult *result,
                                         GError      **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}