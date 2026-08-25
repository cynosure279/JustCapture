/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef JUSTCAPTURE_FILENAME_H
#define JUSTCAPTURE_FILENAME_H

#include <gio/gio.h>
#include "justcapture/errors.h"

G_BEGIN_DECLS

/* Generate screenshot basename: "Screenshot_2026-08-24_02-30-14" (no extension). */
gchar *just_capture_filename_make_screenshot (GDateTime *timestamp);

/* Generate recording basename: "ScreenRecording_2026-08-24_02-30-14" (no extension). */
gchar *just_capture_filename_make_recording (GDateTime *timestamp);

/* Find a unique filename in the directory by appending _1, _2, etc.
 * If extension is NULL, just the basename is used.
 * Returns the full path, or NULL with error set. */
gchar *just_capture_filename_make_unique (const gchar *directory,
                                          const gchar *basename,
                                          const gchar *extension,
                                          GError     **error);

G_END_DECLS

#endif /* JUSTCAPTURE_FILENAME_H */