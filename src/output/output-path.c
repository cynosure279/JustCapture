/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/output-path.h"

/**
 * just_capture_output_path_get_screenshots_dir:
 *
 * Returns the path to the standard screenshot directory:
 * $XDG_PICTURES_DIR/Screenshots/
 *
 * Caller owns the returned string.
 */
gchar *
just_capture_output_path_get_screenshots_dir (void)
{
  const gchar *pictures = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures == NULL)
    pictures = g_build_filename (g_get_home_dir (), "Pictures", NULL);

  return g_build_filename (pictures, "Screenshots", NULL);
}

/**
 * just_capture_output_path_get_recordings_dir:
 *
 * Returns the path to the standard recording directory:
 * $XDG_VIDEOS_DIR/Screen Recordings/
 *
 * Caller owns the returned string.
 */
gchar *
just_capture_output_path_get_recordings_dir (void)
{
  const gchar *videos = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
  if (videos == NULL)
    videos = g_build_filename (g_get_home_dir (), "Videos", NULL);

  return g_build_filename (videos, "Screen Recordings", NULL);
}

/**
 * just_capture_output_path_ensure_dir:
 * @dir: directory path
 * @error: (nullable): return location for error
 *
 * Creates the directory and all its parents with 0755.
 *
 * Returns: %TRUE on success
 */
gboolean
just_capture_output_path_ensure_dir (const gchar *dir,
                                     GError     **error)
{
  g_return_val_if_fail (dir != NULL, FALSE);

  if (g_mkdir_with_parents (dir, 0755) < 0)
    {
      g_set_error (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_IO,
                   "Failed to create directory '%s': %s",
                   dir, g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/**
 * just_capture_output_path_make:
 * @kind: output kind (screenshot or recording)
 * @filename: the filename (e.g. "Screenshot_2026-08-24_02-30-14.png")
 *
 * Combines the standard output directory with @filename.
 *
 * Returns: (transfer full): the full path, caller-owned
 */
gchar *
just_capture_output_path_make (JustCaptureOutputKind  kind,
                               const gchar           *filename)
{
  g_return_val_if_fail (filename != NULL, NULL);

  g_autofree gchar *dir = NULL;
  if (kind == JUST_CAPTURE_OUTPUT_KIND_SCREENSHOT)
    dir = just_capture_output_path_get_screenshots_dir ();
  else
    dir = just_capture_output_path_get_recordings_dir ();

  return g_build_filename (dir, filename, NULL);
}
