/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_OUTPUT_PATH_H
#define JUSTCAPTURE_OUTPUT_PATH_H

#include <gio/gio.h>
#include "justcapture/types.h"
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Get the standard screenshot directory (Pictures/Screenshots/). */
gchar *just_capture_output_path_get_screenshots_dir (void);

/* Get the standard recording directory (Videos/Screen Recordings/). */
gchar *just_capture_output_path_get_recordings_dir (void);

/* Ensure the directory exists (mkdir -p with 0755). */
gboolean just_capture_output_path_ensure_dir (const gchar *dir,
                                              GError     **error);

/* Build a full output path from output kind and filename.
 * The caller owns the returned string. */
gchar *just_capture_output_path_make (JustCaptureOutputKind  kind,
                                      const gchar           *filename);

G_END_DECLS

#endif /* JUSTCAPTURE_OUTPUT_PATH_H */
