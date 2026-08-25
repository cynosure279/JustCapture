/*
 * Copyright (C) 2026 JustCapture Contributors
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "justcapture/filename.h"
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

static void
test_screenshot_basename (void)
{
  /* Create a fixed local GDateTime: 2026-08-24 02:30:14 */
  g_autoptr(GDateTime) dt = g_date_time_new_local (2026, 8, 24, 2, 30, 14);
  g_autofree gchar *name = just_capture_filename_make_screenshot (dt);
  g_assert_nonnull (name);
  g_assert_cmpstr (name, ==, "Screenshot_2026-08-24_02-30-14");
}

static void
test_recording_basename (void)
{
  g_autoptr(GDateTime) dt = g_date_time_new_local (2026, 8, 24, 2, 30, 14);
  g_autofree gchar *name = just_capture_filename_make_recording (dt);
  g_assert_nonnull (name);
  g_assert_cmpstr (name, ==, "ScreenRecording_2026-08-24_02-30-14");
}

static void
test_unique_first (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  g_autofree gchar *path = just_capture_filename_make_unique (tmpdir, "testfile", ".txt", NULL);
  g_assert_nonnull (path);
  g_assert_true (g_str_has_suffix (path, "testfile.txt"));
  g_remove (path);
  g_rmdir (tmpdir);
}

static void
test_unique_with_collision (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  /* Create first file to force collision */
  g_autofree gchar *first = g_build_filename (tmpdir, "collision.txt", NULL);
  g_file_set_contents (first, "data", -1, NULL);

  g_autofree gchar *path = just_capture_filename_make_unique (tmpdir, "collision", ".txt", NULL);
  g_assert_nonnull (path);
  g_assert_true (g_str_has_suffix (path, "collision_1.txt"));

  g_remove (first);
  g_remove (path);
  g_rmdir (tmpdir);
}

static void
test_unique_no_extension (void)
{
  g_autofree gchar *tmpdir = g_dir_make_tmp ("justcapture-test-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  g_autofree gchar *path = just_capture_filename_make_unique (tmpdir, "testfile", NULL, NULL);
  g_assert_nonnull (path);
  g_assert_true (g_str_has_suffix (path, "testfile"));

  g_remove (path);
  g_rmdir (tmpdir);
}

static void
test_unique_null_timestamp (void)
{
  g_autofree gchar *name = just_capture_filename_make_screenshot (NULL);
  g_assert_nonnull (name);
  g_assert_true (g_str_has_prefix (name, "Screenshot_"));
  g_assert_nonnull (strstr (name, "_"));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/filename/screenshot_basename", test_screenshot_basename);
  g_test_add_func ("/filename/recording_basename", test_recording_basename);
  g_test_add_func ("/filename/unique_first", test_unique_first);
  g_test_add_func ("/filename/unique_collision", test_unique_with_collision);
  g_test_add_func ("/filename/unique_no_extension", test_unique_no_extension);
  g_test_add_func ("/filename/null_timestamp", test_unique_null_timestamp);
  return g_test_run ();
}