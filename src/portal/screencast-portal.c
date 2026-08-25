/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * ScreenCast Portal wrapper.
 *
 * Implements the full CreateSession → SelectSources → Start →
 * OpenPipeWireRemote flow and returns a #JustCaptureScreenCastSession
 * containing the PipeWire fd, stream descriptors, and restore token.
 */

#include "justcapture/screencast-portal.h"
#include "justcapture/portal-request.h"

#include <fcntl.h>
#include <unistd.h>
#include <gio/gunixfdlist.h>

/* ─────────────────────────────────────────
 *  Session creation state machine
 * ───────────────────────────────────────── */

typedef struct {
  JustCapturePortal            *portal;
  JustCaptureScreenCastSession *session;
  GTask                        *task;
  GCancellable                 *cancellable;
  gchar                        *parent_window;
  guint                         source_types;
  JustCaptureCursorMode         cursor_mode;
  gboolean                      allow_multiple;
  gchar                        *restore_token;
  /* Intermediate state */
  gchar                        *session_handle;
} CreateSessionData;

static void do_select_sources (CreateSessionData *data);
static void do_start (CreateSessionData *data);
static void do_open_pipewire (CreateSessionData *data);

static CreateSessionData *
create_session_data_new (JustCapturePortal            *portal,
                         guint                         source_types,
                         JustCaptureCursorMode          cursor_mode,
                         gboolean                       allow_multiple,
                         const gchar                   *restore_token,
                         GCancellable                  *cancellable,
                         GAsyncReadyCallback            callback,
                         gpointer                       user_data)
{
  CreateSessionData *data = g_new0 (CreateSessionData, 1);
  data->portal = portal;
  data->source_types = source_types;
  data->cursor_mode = cursor_mode;
  data->allow_multiple = allow_multiple;
  data->restore_token = g_strdup (restore_token);
  data->cancellable = cancellable;
  data->task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (data->task, just_capture_screencast_session_create_async);
  data->session = g_new0 (JustCaptureScreenCastSession, 1);
  data->session->pipewire_fd = -1;
  return data;
}

static void
create_session_data_free (CreateSessionData *data)
{
  if (data == NULL)
    return;

  g_free (data->session_handle);
  g_free (data->restore_token);
  g_free (data->parent_window);

  if (data->task != NULL)
    g_object_unref (data->task);

  g_free (data);
}

static void
fail_session (CreateSessionData *data,
              GError            *error)
{
  if (g_task_had_error (data->task))
    {
      g_error_free (error);
      return;
    }

  g_task_return_error (data->task, error);
  g_clear_pointer (&data->session, just_capture_screencast_session_free);
  data->session = NULL;
  create_session_data_free (data);
}


/* ── Sync portal call helper ────────────────────────────────────────────
 * Works around the async portal-request.c race condition by doing a
 * synchronous method call + GMainLoop wait for the Response signal.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
  GMainLoop *loop;
  guint code;
  GVariant *results;
} SyncResponseData;

static void
sync_on_response (GDBusConnection *connection G_GNUC_UNUSED,
                  const gchar     *sender_name G_GNUC_UNUSED,
                  const gchar     *object_path G_GNUC_UNUSED,
                  const gchar     *interface_name G_GNUC_UNUSED,
                  const gchar     *signal_name G_GNUC_UNUSED,
                  GVariant        *parameters,
                  gpointer         user_data)
{
  SyncResponseData *data = user_data;
  if (data->results != NULL)
    return;  /* already got a response */
  g_variant_get (parameters, "(u@a{sv})", &data->code, &data->results);
  g_main_loop_quit (data->loop);
}

/* Call a portal method synchronously and wait for the Response signal.
 * Returns the Response results (a{sv}), or NULL with error set.
 * @parameters is consumed. */
static GVariant *
portal_call_sync (GDBusConnection  *conn,
                  const gchar      *method_name,
                  GVariant         *parameters,
                  GError          **error)
{
  GVariant *reply = g_dbus_connection_call_sync (conn,
      "org.freedesktop.portal.Desktop",
      "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.ScreenCast",
      method_name, parameters,
      G_VARIANT_TYPE ("(o)"), G_DBUS_CALL_FLAGS_NONE, 15000, NULL, error);
  if (reply == NULL)
    return NULL;

  const gchar *req_path = NULL;
  g_variant_get (reply, "(&o)", &req_path);
  g_variant_unref (reply);

  if (req_path == NULL)
    {
      g_set_error (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL,
                   "No request path in reply");
      return NULL;
    }

  SyncResponseData srd = {0};
  /* Use a dedicated GMainContext so the signal is dispatched here,
   * not on the main thread's default context (race condition fix). */
  GMainContext *ctx = g_main_context_new ();
  g_main_context_push_thread_default (ctx);
  srd.loop = g_main_loop_new (ctx, FALSE);
  guint sub = g_dbus_connection_signal_subscribe (conn, NULL,
      "org.freedesktop.portal.Request", "Response", req_path, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, sync_on_response, &srd, NULL);

  g_main_loop_run (srd.loop);
  g_dbus_connection_signal_unsubscribe (conn, sub);
  g_main_loop_unref (srd.loop);
  g_main_context_pop_thread_default (ctx);
  g_main_context_unref (ctx);

  if (srd.results == NULL)
    {
      g_set_error (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED,
                   "No Response signal received for %s", method_name);
      return NULL;
    }

  if (srd.code != 0)
    {
      g_variant_unref (srd.results);
      g_set_error (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_FAILED,
                   "Portal %s failed with code %u", method_name, srd.code);
      return NULL;
    }

  return srd.results;  /* caller owns */
}

/* ─────────────────────────────────────────
 *  Step 1: CreateSession
 * ───────────────────────────────────────── */

static void
on_create_session (GObject      *source_object G_GNUC_UNUSED,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  CreateSessionData *data = user_data;
  GError *error = NULL;

  GVariant *results = just_capture_portal_request_call_finish (res, &error);
  if (results == NULL)
    {
      fail_session (data, error);
      return;
    }

  /* Debug: print what the portal returned */
  {
    gchar *debug_str = g_variant_print (results, TRUE);
    g_message ("CreateSession results: %s", debug_str);
    g_free (debug_str);
  }

  /* Extract session_handle — works for both 'o' and 's' types */
  const gchar *session_handle = NULL;
  {
    GVariant *sh = g_variant_lookup_value (results, "session_handle", NULL);
    if (sh)
      {
        session_handle = g_variant_get_string (sh, NULL);
        /* session_handle is valid while sh is alive — copy before unref */
        session_handle = g_strdup (session_handle);
        g_variant_unref (sh);
      }
  }
  if (session_handle == NULL)
    {
      g_variant_unref (results);
      fail_session (data, g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PROTOCOL,
                                       "CreateSession reply missing session_handle"));
      return;
    }

  data->session_handle = g_strdup (session_handle);
  data->session->session_handle = g_strdup (session_handle);
  g_variant_unref (results);

  do_select_sources (data);
}

static void
do_create_session (CreateSessionData *data)
{
  /* Build options with handle_token AND session_handle_token */
  GVariant *base_opts = just_capture_portal_build_options (data->portal);
  GVariantDict dict;
  g_variant_dict_init (&dict, base_opts);
  g_autofree gchar *session_token = g_strdup_printf ("justcapture_session_%u", g_random_int ());
  g_variant_dict_insert (&dict, "session_handle_token", "s", session_token);
  GVariant *opts = g_variant_ref_sink (g_variant_dict_end (&dict));
  g_variant_unref (base_opts);
  GVariant *params = g_variant_new ("(@a{sv})", opts);

  just_capture_portal_request_call (data->portal,
                                    "org.freedesktop.portal.ScreenCast",
                                    "CreateSession",
                                    params,
                                    120000,
                                    data->cancellable,
                                    on_create_session,
                                    data);
}

/* ─────────────────────────────────────────
 *  Step 2: SelectSources
 * ───────────────────────────────────────── */

static void
on_select_sources (GObject      *source_object G_GNUC_UNUSED,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  CreateSessionData *data = user_data;
  GError *error = NULL;

  GVariant *results = just_capture_portal_request_call_finish (res, &error);
  if (results == NULL)
    {
      fail_session (data, error);
      return;
    }

  g_variant_unref (results);
  do_start (data);
}

static void
do_select_sources (CreateSessionData *data)
{
  GVariant *opts = g_variant_ref_sink (just_capture_portal_build_options (data->portal));
  GVariantDict dict;
  g_variant_dict_init (&dict, opts);
  g_variant_dict_insert (&dict, "types", "u", data->source_types);
  g_variant_dict_insert (&dict, "multiple", "b", data->allow_multiple ? TRUE : FALSE);
  g_variant_dict_insert (&dict, "cursor_mode", "u", (guint) data->cursor_mode);
  if (data->restore_token != NULL)
    g_variant_dict_insert (&dict, "restore_token", "s", data->restore_token);
  g_variant_dict_insert (&dict, "persist_mode", "u", 1);
  GVariant *options = g_variant_ref_sink (g_variant_dict_end (&dict));
  g_variant_unref (opts);

  GVariant *params = g_variant_new ("(o@a{sv})", data->session_handle, options);

  just_capture_portal_request_call (data->portal,
                                    "org.freedesktop.portal.ScreenCast",
                                    "SelectSources",
                                    params,
                                    120000,
                                    data->cancellable,
                                    on_select_sources,
                                    data);
}

/* ─────────────────────────────────────────
 *  Step 3: Start
 * ───────────────────────────────────────── */

static void
parse_streams (GVariant *streams_array, JustCaptureScreenCastSession *session)
{
  GVariantIter iter;
  g_variant_iter_init (&iter, streams_array);

  guint node_id;
  GVariant *props_variant;
  while (g_variant_iter_next (&iter, "(u@a{sv})", &node_id, &props_variant))
    {
      JustCaptureStreamDescriptor *desc = g_new0 (JustCaptureStreamDescriptor, 1);
      desc->node_id = (gint) node_id;

      GVariantDict props;
      g_variant_dict_init (&props, props_variant);

      /* id (s) */
      const gchar *id = NULL;
      if (g_variant_dict_lookup (&props, "id", "&s", &id))
        desc->id = g_strdup (id);

      /* position (ii) */
      GVariant *pos = g_variant_dict_lookup_value (&props, "position", G_VARIANT_TYPE ("(ii)"));
      if (pos != NULL)
        {
          g_variant_get (pos, "(ii)", &desc->position_x, &desc->position_y);
          g_variant_unref (pos);
        }
      else
        {
          /* Fallback: try (ai) - older format */
          pos = g_variant_dict_lookup_value (&props, "position", G_VARIANT_TYPE ("ai"));
          if (pos != NULL)
            {
              gsize n;
              const gint *arr = g_variant_get_fixed_array (pos, &n, sizeof (gint));
              if (n >= 2) { desc->position_x = arr[0]; desc->position_y = arr[1]; }
              g_variant_unref (pos);
            }
        }

      /* size (ii) */
      GVariant *sz = g_variant_dict_lookup_value (&props, "size", G_VARIANT_TYPE ("(ii)"));
      if (sz != NULL)
        {
          g_variant_get (sz, "(ii)", &desc->width, &desc->height);
          g_variant_unref (sz);
        }
      else
        {
          sz = g_variant_dict_lookup_value (&props, "size", G_VARIANT_TYPE ("ai"));
          if (sz != NULL)
            {
              gsize n;
              const gint *arr = g_variant_get_fixed_array (sz, &n, sizeof (gint));
              if (n >= 2) { desc->width = arr[0]; desc->height = arr[1]; }
              g_variant_unref (sz);
            }
        }

      /* source_type (u) */
      g_variant_dict_lookup (&props, "source_type", "u", &desc->source_type);

      /* pipewire-serial (t) - v6+ */
      guint64 serial = 0;
      if (g_variant_dict_lookup (&props, "pipewire-serial", "t", &serial))
        {
          desc->pipewire_serial = serial;
          session->has_serial = TRUE;
        }

      g_variant_dict_clear (&props);
      g_variant_unref (props_variant);

      session->streams = g_list_append (session->streams, desc);
    }
}

static void
on_start (GObject      *source_object G_GNUC_UNUSED,
          GAsyncResult *res,
          gpointer      user_data)
{
  CreateSessionData *data = user_data;
  GError *error = NULL;

  GVariant *results = just_capture_portal_request_call_finish (res, &error);
  if (results == NULL)
    {
      fail_session (data, error);
      return;
    }

  /* Streams */
  GVariant *streams_v = NULL;
  if (g_variant_lookup (results, "streams", "@a(ua{sv})", &streams_v))
    {
      parse_streams (streams_v, data->session);
      g_variant_unref (streams_v);
    }

  /* Restore token */
  const gchar *token = NULL;
  if (g_variant_lookup (results, "restore_token", "&s", &token))
    data->session->restore_token = g_strdup (token);

  g_variant_unref (results);

  do_open_pipewire (data);
}

static void
do_start (CreateSessionData *data)
{
  GVariant *opts = g_variant_ref_sink (just_capture_portal_build_options (data->portal));
  GVariant *params = g_variant_new ("(osa{sv})",
                                     data->session_handle,
                                     data->parent_window ? data->parent_window : "",
                                     opts);

  just_capture_portal_request_call (data->portal,
                                    "org.freedesktop.portal.ScreenCast",
                                    "Start",
                                    params,
                                    120000,
                                    data->cancellable,
                                    on_start,
                                    data);
}

/* ─────────────────────────────────────────
 *  Step 4: OpenPipeWireRemote
 * ───────────────────────────────────────── */

static void
on_open_pipewire (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  CreateSessionData *data = user_data;
  GError *error = NULL;
  GUnixFDList *fd_list = NULL;

  g_autoptr(GVariant) result = g_dbus_connection_call_with_unix_fd_list_finish (
      G_DBUS_CONNECTION (source_object), &fd_list, res, &error);
  if (result == NULL)
    {
      fail_session (data, error);
      return;
    }

  /* The reply variant is (h) — an index into the fd list, not the fd itself. */
  gint idx = g_variant_get_handle (result);
  gint fd = g_unix_fd_list_get (fd_list, idx, &error);
  g_object_unref (fd_list);
  if (fd < 0)
    {
      fail_session (data, error);
      return;
    }

  /* Dup to a high fd with CLOEXEC so it survives child processes. */
  gint dup_fd = fcntl (fd, F_DUPFD, 3);
  close (fd);
  if (dup_fd >= 0)
    fcntl (dup_fd, F_SETFD, FD_CLOEXEC);
  data->session->pipewire_fd = dup_fd;

  /* Now complete the task */
  g_task_return_pointer (data->task, data->session, NULL);
  data->session = NULL;  /* ownership transferred */
  create_session_data_free (data);
}

static void
do_open_pipewire (CreateSessionData *data)
{
  GDBusConnection *conn = just_capture_portal_get_connection (data->portal);
  if (conn == NULL)
    {
      fail_session (data, g_error_new (JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE,
                                       "Session bus not available"));
      return;
    }

  /* OpenPipeWireRemote returns a file descriptor via the D-Bus fd-passing
   * mechanism, so we must use the _with_unix_fd_list variant. */
  g_dbus_connection_call_with_unix_fd_list (conn,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.ScreenCast",
                          "OpenPipeWireRemote",
                          g_variant_new ("(o)", data->session_handle),
                          G_VARIANT_TYPE ("(h)"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL,  /* fd_list to send (none needed) */
                          data->cancellable,
                          on_open_pipewire,
                          data);
}

/* ─────────────────────────────────────────
 *  Public API: session create
 * ───────────────────────────────────────── */

/* Sync worker thread that runs the full portal flow */
static void
screencast_create_thread (GTask        *task,
                          gpointer      source_object G_GNUC_UNUSED,
                          gpointer      task_data,
                          GCancellable *cancellable G_GNUC_UNUSED)
{
  CreateSessionData *data = task_data;
  GError *error = NULL;
  GDBusConnection *conn = just_capture_portal_get_connection (data->portal);
  if (conn == NULL)
    {
      g_task_return_new_error (task, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE,
                                 "Session bus not available");
      create_session_data_free (data);
      return;
    }

  /* Step 1: CreateSession */
  GVariantBuilder b;
  g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  g_autofree gchar *token1 = g_strdup_printf ("justcapture%u", g_random_int ());
  g_autofree gchar *stoken = g_strdup_printf ("justcapture_session_%u", g_random_int ());
  g_variant_builder_add (&b, "{sv}", "handle_token", g_variant_new_string (token1));
  g_variant_builder_add (&b, "{sv}", "session_handle_token", g_variant_new_string (stoken));
  GVariant *results = portal_call_sync (conn, "CreateSession",
      g_variant_new ("(@a{sv})", g_variant_builder_end (&b)), &error);
  if (results == NULL)
    { g_task_return_error (task, error); create_session_data_free (data); return; }

  const gchar *session_handle = NULL;
  g_variant_lookup (results, "session_handle", "&s", &session_handle);
  data->session_handle = g_strdup (session_handle);
  data->session->session_handle = g_strdup (session_handle);
  g_variant_unref (results);

  /* Step 2: SelectSources */
  GVariantBuilder b2;
  g_variant_builder_init (&b2, G_VARIANT_TYPE ("a{sv}"));
  g_autofree gchar *token2 = g_strdup_printf ("justcapture%u", g_random_int ());
  g_variant_builder_add (&b2, "{sv}", "handle_token", g_variant_new_string (token2));
  g_variant_builder_add (&b2, "{sv}", "types", g_variant_new_uint32 (data->source_types));
  g_variant_builder_add (&b2, "{sv}", "multiple", g_variant_new_boolean (data->allow_multiple));
  g_variant_builder_add (&b2, "{sv}", "cursor_mode", g_variant_new_uint32 ((guint) data->cursor_mode));
  if (data->restore_token != NULL)
    g_variant_builder_add (&b2, "{sv}", "restore_token", g_variant_new_string (data->restore_token));
  g_variant_builder_add (&b2, "{sv}", "persist_mode", g_variant_new_uint32 (1));
  results = portal_call_sync (conn, "SelectSources",
      g_variant_new ("(o@a{sv})", data->session_handle, g_variant_builder_end (&b2)), &error);
  if (results == NULL)
    { g_task_return_error (task, error); create_session_data_free (data); return; }
  g_variant_unref (results);

  /* Step 3: Start */
  GVariantBuilder b3;
  g_variant_builder_init (&b3, G_VARIANT_TYPE ("a{sv}"));
  g_autofree gchar *token3 = g_strdup_printf ("justcapture%u", g_random_int ());
  g_variant_builder_add (&b3, "{sv}", "handle_token", g_variant_new_string (token3));
  results = portal_call_sync (conn, "Start",
      g_variant_new ("(os@a{sv})", data->session_handle, "", g_variant_builder_end (&b3)), &error);
  if (results == NULL)
    { g_task_return_error (task, error); create_session_data_free (data); return; }

  /* Parse streams */
  GVariant *streams_v = NULL;
  if (g_variant_lookup (results, "streams", "@a(ua{sv})", &streams_v))
    {
      parse_streams (streams_v, data->session);
      g_variant_unref (streams_v);
    }
  const gchar *rtoken = NULL;
  if (g_variant_lookup (results, "restore_token", "&s", &rtoken))
    data->session->restore_token = g_strdup (rtoken);
  g_variant_unref (results);

  /* Step 4: OpenPipeWireRemote */
  GUnixFDList *fd_list = NULL;
  GVariant *pw_reply = g_dbus_connection_call_with_unix_fd_list_sync (conn,
      "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.ScreenCast", "OpenPipeWireRemote",
      g_variant_new ("(o@a{sv})", data->session_handle,
                     g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0)),
      G_VARIANT_TYPE ("(h)"), G_DBUS_CALL_FLAGS_NONE, -1,
      NULL, &fd_list, NULL, &error);
  if (pw_reply == NULL)
    { g_task_return_error (task, error); create_session_data_free (data); return; }

  gint idx;
  g_variant_get (pw_reply, "(h)", &idx);
  gint fd = g_unix_fd_list_get (fd_list, idx, NULL);
  gint dup_fd = fcntl (fd, F_DUPFD, 3);
  close (fd);
  if (dup_fd >= 0)
    fcntl (dup_fd, F_SETFD, FD_CLOEXEC);
  data->session->pipewire_fd = dup_fd;

  g_variant_unref (pw_reply);
  g_object_unref (fd_list);

  /* Success! */
  g_task_return_pointer (task, data->session, NULL);
  data->session = NULL;  /* ownership transferred */
  create_session_data_free (data);
}

void
just_capture_screencast_session_create_async (
    JustCapturePortal    *portal,
    guint                 source_types,
    JustCaptureCursorMode cursor_mode,
    gboolean              allow_multiple,
    const gchar          *restore_token,
    GCancellable         *cancellable,
    GAsyncReadyCallback   callback,
    gpointer              user_data)
{
  g_return_if_fail (portal != NULL);
  g_return_if_fail (source_types != 0);

  CreateSessionData *data = create_session_data_new (portal, source_types,
                                                     cursor_mode, allow_multiple,
                                                     restore_token, cancellable,
                                                     callback, user_data);
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, data, NULL);  /* freed manually in thread func */
  g_task_run_in_thread (task, screencast_create_thread);
  g_object_unref (task);
}

JustCaptureScreenCastSession *
just_capture_screencast_session_create_finish (
    JustCapturePortal  *portal G_GNUC_UNUSED,
    GAsyncResult       *result,
    GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/* ─────────────────────────────────────────
 *  Public API: session close
 * ───────────────────────────────────────── */

typedef struct {
  GTask *task;
  JustCaptureScreenCastSession *session;
} CloseSessionData;

static void
on_close_session (GObject      *source_object G_GNUC_UNUSED,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  CloseSessionData *data = user_data;
  GError *error = NULL;

  GVariant *results = just_capture_portal_request_call_finish (res, &error);
  if (results == NULL)
    {
      g_task_return_error (data->task, error);
    }
  else
    {
      g_variant_unref (results);
      g_task_return_boolean (data->task, TRUE);
    }

  just_capture_screencast_session_free (data->session);
  g_object_unref (data->task);
  g_free (data);
}

void
just_capture_screencast_session_close_async (
    JustCapturePortal            *portal,
    JustCaptureScreenCastSession *session,
    GCancellable                 *cancellable,
    GAsyncReadyCallback           callback,
    gpointer                      user_data)
{
  g_return_if_fail (portal != NULL);
  g_return_if_fail (session != NULL);

  CloseSessionData *data = g_new0 (CloseSessionData, 1);
  data->task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (data->task, just_capture_screencast_session_close_async);
  data->session = session;

  GVariant *opts = g_variant_ref_sink (just_capture_portal_build_options (portal));
  GVariant *params = g_variant_new ("(o@a{sv})", session->session_handle, opts);

  just_capture_portal_request_call (portal,
                                    "org.freedesktop.portal.Session",
                                    "Close",
                                    params,
                                    10000,
                                    cancellable,
                                    on_close_session,
                                    data);
}

gboolean
just_capture_screencast_session_close_finish (
    JustCapturePortal  *portal G_GNUC_UNUSED,
    GAsyncResult       *result,
    GError            **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}