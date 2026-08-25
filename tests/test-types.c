/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/types.h"
#include <unistd.h>

static void
test_screenshot_result_create_free (void)
{
  JustCaptureScreenshotResult *r = g_new0 (JustCaptureScreenshotResult, 1);
  r->uri = g_strdup ("file:///tmp/test.png");
  r->actual_target = 4;
  just_capture_screenshot_result_free (r);
  /* Should not crash */
}

static void
test_screenshot_result_autoptr (void)
{
  g_autoptr(JustCaptureScreenshotResult) r = g_new0 (JustCaptureScreenshotResult, 1);
  r->uri = g_strdup ("file:///tmp/test.png");
  /* Should be freed on scope exit */
}

static void
test_stream_descriptor_create_free (void)
{
  JustCaptureStreamDescriptor *d = g_new0 (JustCaptureStreamDescriptor, 1);
  d->id = g_strdup ("stream1");
  d->node_id = 42;
  d->pipewire_serial = 12345;
  d->source_type = 1;
  d->position_x = 0;
  d->position_y = 0;
  d->width = 1920;
  d->height = 1080;
  just_capture_stream_descriptor_free (d);
}

static void
test_screencast_session_create_free (void)
{
  JustCaptureScreenCastSession *s = g_new0 (JustCaptureScreenCastSession, 1);
  s->session_handle = g_strdup ("/org/freedesktop/portal/desktop/session/1");
  s->pipewire_fd = -1;
  s->restore_token = g_strdup ("token123");
  s->has_serial = TRUE;
  just_capture_screencast_session_free (s);
}

static void
test_screencast_session_take_fd (void)
{
  JustCaptureScreenCastSession *s = g_new0 (JustCaptureScreenCastSession, 1);
  s->session_handle = g_strdup ("/session/1");
  gint pipe_fds[2];
  g_assert_true (pipe (pipe_fds) == 0);
  s->pipewire_fd = pipe_fds[0];

  gint fd = just_capture_screencast_session_take_fd (s);
  g_assert_cmpint (fd, ==, pipe_fds[0]);
  g_assert_cmpint (s->pipewire_fd, ==, -1);
  close (fd);
  close (pipe_fds[1]);
  just_capture_screencast_session_free (s);
}

static void
test_capabilities_create_free (void)
{
  JustCaptureCapabilities *c = g_new0 (JustCaptureCapabilities, 1);
  c->portal_available = TRUE;
  c->screenshot_targets = JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN | JUST_CAPTURE_SCREENSHOT_TARGET_WINDOW;
  c->screencast_source_types = JUST_CAPTURE_SOURCETYPE_MONITOR;
  c->screenshot_portal_version = 3;
  c->screencast_portal_version = 6;
  just_capture_capabilities_free (c);
}

static void
test_enum_values (void)
{
  g_assert_cmpint (JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN, ==, 1);
  g_assert_cmpint (JUST_CAPTURE_SCREENSHOT_TARGET_WINDOW, ==, 2);
  g_assert_cmpint (JUST_CAPTURE_SCREENSHOT_TARGET_AREA, ==, 4);
  g_assert_cmpint (JUST_CAPTURE_SCREENSHOT_TARGET_ACTIVE_WINDOW, ==, 8);

  g_assert_cmpint (JUST_CAPTURE_SOURCETYPE_MONITOR, ==, 1);
  g_assert_cmpint (JUST_CAPTURE_SOURCETYPE_WINDOW, ==, 2);
  g_assert_cmpint (JUST_CAPTURE_SOURCETYPE_VIRTUAL, ==, 4);

  g_assert_cmpint (JUST_CAPTURE_CURSOR_MODE_HIDDEN, ==, 1);
  g_assert_cmpint (JUST_CAPTURE_CURSOR_MODE_EMBEDDED, ==, 2);
  g_assert_cmpint (JUST_CAPTURE_CURSOR_MODE_METADATA, ==, 4);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/types/screenshot_result_create_free", test_screenshot_result_create_free);
  g_test_add_func ("/types/screenshot_result_autoptr", test_screenshot_result_autoptr);
  g_test_add_func ("/types/stream_descriptor_create_free", test_stream_descriptor_create_free);
  g_test_add_func ("/types/screencast_session_create_free", test_screencast_session_create_free);
  g_test_add_func ("/types/screencast_session_take_fd", test_screencast_session_take_fd);
  g_test_add_func ("/types/capabilities_create_free", test_capabilities_create_free);
  g_test_add_func ("/types/enum_values", test_enum_values);
  return g_test_run ();
}
