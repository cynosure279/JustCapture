/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/types.h"

/**
 * just_capture_screenshot_result_free:
 * @result: a #JustCaptureScreenshotResult
 *
 * Frees a screenshot result and all of its resources.
 */
void
just_capture_screenshot_result_free (JustCaptureScreenshotResult *result)
{
  if (result == NULL)
    return;

  g_free (result->uri);
  g_free (result);
}

/**
 * just_capture_stream_descriptor_free:
 * @desc: a #JustCaptureStreamDescriptor
 */
void
just_capture_stream_descriptor_free (JustCaptureStreamDescriptor *desc)
{
  if (desc == NULL)
    return;

  g_free (desc->id);
  g_free (desc);
}

/**
 * just_capture_screencast_session_free:
 * @session: a #JustCaptureScreenCastSession
 *
 * Frees the session, its streams and closes the PipeWire fd
 * if it is still owned by the session.
 */
void
just_capture_screencast_session_free (JustCaptureScreenCastSession *session)
{
  if (session == NULL)
    return;

  if (session->pipewire_fd >= 0)
    close (session->pipewire_fd);

  g_list_free_full (session->streams, (GDestroyNotify) just_capture_stream_descriptor_free);
  g_free (session->session_handle);
  g_free (session->restore_token);
  g_free (session);
}

/**
 * just_capture_screencast_session_take_fd:
 * @session: a #JustCaptureScreenCastSession
 *
 * Transfers ownership of the PipeWire fd out of the session.
 * The caller is responsible for closing the returned fd.
 *
 * Returns: the fd, or -1 if the session does not own one
 */
gint
just_capture_screencast_session_take_fd (JustCaptureScreenCastSession *session)
{
  g_return_val_if_fail (session != NULL, -1);

  gint fd = session->pipewire_fd;
  session->pipewire_fd = -1;
  return fd;
}

/**
 * just_capture_capabilities_free:
 * @caps: a #JustCaptureCapabilities
 */
void
just_capture_capabilities_free (JustCaptureCapabilities *caps)
{
  g_free (caps);
}
