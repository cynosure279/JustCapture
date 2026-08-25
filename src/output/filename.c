/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/filename.h"

/**
 * just_capture_filename_make_screenshot:
 * @timestamp: (nullable): a #GDateTime, or %NULL for "now"
 *
 * Generates a basename: "Screenshot_2026-08-24_02-30-14"
 * (no extension — the caller should add one via make_unique).
 *
 * Returns: (transfer full): a newly allocated string
 */
gchar *
just_capture_filename_make_screenshot (GDateTime *timestamp)
{
  g_autoptr(GDateTime) ts = timestamp
    ? g_date_time_to_local (timestamp)
    : g_date_time_new_now_local ();

  return g_date_time_format (ts, "Screenshot_%Y-%m-%d_%H-%M-%S");
}

/**
 * just_capture_filename_make_recording:
 * @timestamp: (nullable): a #GDateTime, or %NULL for "now"
 *
 * Generates a basename: "ScreenRecording_2026-08-24_02-30-14"
 * (no extension).
 *
 * Returns: (transfer full): a newly allocated string
 */
gchar *
just_capture_filename_make_recording (GDateTime *timestamp)
{
  g_autoptr(GDateTime) ts = timestamp
    ? g_date_time_to_local (timestamp)
    : g_date_time_new_now_local ();

  return g_date_time_format (ts, "ScreenRecording_%Y-%m-%d_%H-%M-%S");
}

/**
 * just_capture_filename_make_unique:
 * @directory: path to the target directory
 * @basename: base name without extension
 * @extension: (nullable): file extension including dot, e.g. ".png"
 * @error: (nullable): return location for error
 *
 * Finds a non-existing file path in @directory by appending
 * "_1", "_2", ... up to 1000 tries.
 *
 * Returns: (transfer full): the full path, or %NULL on error
 */
gchar *
just_capture_filename_make_unique (const gchar *directory,
                                   const gchar *basename,
                                   const gchar *extension,
                                   GError     **error)
{
  g_return_val_if_fail (directory != NULL, NULL);
  g_return_val_if_fail (basename != NULL, NULL);

  for (gint i = 0; i < 1000; i++)
    {
      g_autofree gchar *name = NULL;
      if (i == 0)
        name = g_strdup_printf ("%s%s", basename, extension ? extension : "");
      else
        name = g_strdup_printf ("%s_%d%s", basename, i, extension ? extension : "");

      gchar *path = g_build_filename (directory, name, NULL);
      if (!g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

      g_free (path);
    }

  g_set_error (error, JUST_CAPTURE_ERROR, JUST_CAPTURE_ERROR_IO,
               "Too many files with basename '%s' in '%s'",
               basename, directory);
  return NULL;
}