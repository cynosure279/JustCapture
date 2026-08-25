/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_TYPES_H
#define JUSTCAPTURE_TYPES_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* ─────────────────────────────────────────
 *  Screenshot Portal target (AvailableTargets bitmask)
 * ───────────────────────────────────────── */

typedef enum {
  JUST_CAPTURE_SCREENSHOT_TARGET_NONE          = 0,
  JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN        = 1 << 0,  /* 1 */
  JUST_CAPTURE_SCREENSHOT_TARGET_WINDOW        = 1 << 1,  /* 2 */
  JUST_CAPTURE_SCREENSHOT_TARGET_AREA          = 1 << 2,  /* 4 */
  JUST_CAPTURE_SCREENSHOT_TARGET_ACTIVE_WINDOW = 1 << 3,  /* 8 */
} JustCaptureScreenshotTarget;

/* ─────────────────────────────────────────
 *  ScreenCast source types (AvailableSourceTypes bitmask)
 * ───────────────────────────────────────── */

typedef enum {
  JUST_CAPTURE_SOURCETYPE_MONITOR = 1 << 0,   /* 1 */
  JUST_CAPTURE_SOURCETYPE_WINDOW = 1 << 1,    /* 2 */
  JUST_CAPTURE_SOURCETYPE_VIRTUAL = 1 << 2,   /* 4 */
} JustCaptureSourceType;

/* ─────────────────────────────────────────
 *  ScreenCast cursor modes (AvailableCursorModes bitmask)
 * ───────────────────────────────────────── */

typedef enum {
  JUST_CAPTURE_CURSOR_MODE_HIDDEN    = 1 << 0,  /* 1 */
  JUST_CAPTURE_CURSOR_MODE_EMBEDDED  = 1 << 1,  /* 2 */
  JUST_CAPTURE_CURSOR_MODE_METADATA  = 1 << 2,  /* 4 */
} JustCaptureCursorMode;

/* ─────────────────────────────────────────
 *  Output kind
 * ───────────────────────────────────────── */

typedef enum {
  JUST_CAPTURE_OUTPUT_KIND_SCREENSHOT,
  JUST_CAPTURE_OUTPUT_KIND_SCREENCAST,
} JustCaptureOutputKind;

/* ─────────────────────────────────────────
 *  Screenshot result
 * ───────────────────────────────────────── */

typedef struct {
  gchar *uri;          /* Portal returned URI (document: or file: scheme) */
  guint  actual_target; /* The target value used */
} JustCaptureScreenshotResult;

void just_capture_screenshot_result_free (JustCaptureScreenshotResult *result);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JustCaptureScreenshotResult, just_capture_screenshot_result_free)

/* ─────────────────────────────────────────
 *  ScreenCast stream descriptor
 * ───────────────────────────────────────── */

typedef struct {
  gchar   *id;               /* Stream unique identifier (v4+, local to session) */
  gint     node_id;          /* PipeWire node ID (v6+ deprecated for targeting) */
  guint64  pipewire_serial;  /* object.serial for PW_KEY_TARGET_OBJECT (v6+) */
  gint     source_type;      /* MONITOR / WINDOW / VIRTUAL */
  gint     position_x, position_y;
  gint     width, height;
} JustCaptureStreamDescriptor;

void just_capture_stream_descriptor_free (JustCaptureStreamDescriptor *desc);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JustCaptureStreamDescriptor, just_capture_stream_descriptor_free)

/* ─────────────────────────────────────────
 *  ScreenCast session
 * ───────────────────────────────────────── */

typedef struct {
  gchar    *session_handle;  /* Portal session handle (object path) */
  gint      pipewire_fd;     /* OpenPipeWireRemote fd (owned by this struct) */
  GList    *streams;         /* GList<JustCaptureStreamDescriptor*> */
  gchar    *restore_token;   /* Restore token for next session (NULL if not available) */
  gboolean  has_serial;      /* TRUE if v6+ pipewire-serial is available */
} JustCaptureScreenCastSession;

void just_capture_screencast_session_free (JustCaptureScreenCastSession *session);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JustCaptureScreenCastSession, just_capture_screencast_session_free)

/* Transfer ownership of the PipeWire fd out of the session.
 * After this call, session->pipewire_fd is set to -1 and the caller
 * is responsible for closing the returned fd. */
gint just_capture_screencast_session_take_fd (JustCaptureScreenCastSession *session);

/* ─────────────────────────────────────────
 *  Capabilities
 * ───────────────────────────────────────── */

typedef struct {
  /* Screenshot available targets (bitmask) */
  JustCaptureScreenshotTarget screenshot_targets;
  /* ScreenCast available source types (bitmask) */
  guint screencast_source_types;
  /* ScreenCast available cursor modes (bitmask) */
  guint screencast_cursor_modes;
  /* Portal version numbers */
  guint screenshot_portal_version;
  guint screencast_portal_version;
  /* Overall portal availability */
  gboolean portal_available;
} JustCaptureCapabilities;

void just_capture_capabilities_free (JustCaptureCapabilities *caps);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JustCaptureCapabilities, just_capture_capabilities_free)

G_END_DECLS

#endif /* JUSTCAPTURE_TYPES_H */
