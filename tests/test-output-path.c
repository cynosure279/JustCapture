/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/output-path.h"
#include <glib/gstdio.h>

static void
test_screenshots_dir (void)
{
  /* Set XDG env to known values */
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_autofree gchar *pics = g_build_filename (tmpdir, "Pictures", NULL);
  g_mkdir_with_parents (pics, 0755);

  g_setenv ("XDG_PICTURES_DIR", pics, TRUE);
  g_autofree gchar *dir = just_capture_output_path_get_screenshots_dir ();
  g_assert_nonnull (dir);
  g_assert_true (g_str_has_suffix (dir, "Screenshots"));

  g_rmdir (pics);
  g_rmdir (tmpdir);
}

static void
test_recordings_dir (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_autofree gchar *vids = g_build_filename (tmpdir, "Videos", NULL);
  g_mkdir_with_parents (vids, 0755);

  g_setenv ("XDG_VIDEOS_DIR", vids, TRUE);
  g_autofree gchar *dir = just_capture_output_path_get_recordings_dir ();
  g_assert_nonnull (dir);
  g_assert_true (g_str_has_suffix (dir, "Screen Recordings"));

  g_rmdir (vids);
  g_rmdir (tmpdir);
}

static void
test_ensure_dir (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_autofree gchar *subdir = g_build_filename (tmpdir, "a", "b", "c", NULL);

  g_assert_true (g_mkdir_with_parents (subdir, 0755) == 0);
  g_assert_true (g_file_test (subdir, G_FILE_TEST_IS_DIR));
  g_rmdir (subdir);
  g_rmdir (g_build_filename (tmpdir, "a", "b", NULL));
  g_rmdir (g_build_filename (tmpdir, "a", NULL));
  g_rmdir (tmpdir);
}

static void
test_make_path (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_autofree gchar *pics = g_build_filename (tmpdir, "Pictures", NULL);
  g_mkdir_with_parents (pics, 0755);
  g_setenv ("XDG_PICTURES_DIR", pics, TRUE);

  g_autofree gchar *path = just_capture_output_path_make (JUST_CAPTURE_OUTPUT_KIND_SCREENSHOT, "test.png");
  g_assert_nonnull (path);
  g_assert_true (g_str_has_suffix (path, "Screenshots/test.png"));

  g_rmdir (pics);
  g_rmdir (tmpdir);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/output_path/screenshots_dir", test_screenshots_dir);
  g_test_add_func ("/output_path/recordings_dir", test_recordings_dir);
  g_test_add_func ("/output_path/ensure_dir", test_ensure_dir);
  g_test_add_func ("/output_path/make_path", test_make_path);
  return g_test_run ();
}
