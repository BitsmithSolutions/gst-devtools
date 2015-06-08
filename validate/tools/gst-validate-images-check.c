/* GStreamer
 *
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 * Copyright (C) 2015 Raspberry Pi Foundation
 *  Author: Thibault Saunier <thibault.saunier@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "../gst-libs/gst/video/gstvalidatessim.h"

#include <gst/gst.h>
#include <gst/validate/validate.h>
#include <gst/video/video.h>

int
main (int argc, char **argv)
{
  GstValidateSsim *ssim;
  gint rep_err, ret = 0;
  GError *err = NULL;
  GstValidateRunner *runner = NULL;
  GOptionContext *ctx;
  const gchar *outfolder = NULL;
  gfloat mssim = 0, lowest = 1, highest = -1;
  gdouble min_avg_similarity = 0.95, min_lowest_similarity = -1.0;

  GOptionEntry options[] = {
    {"min-avg-similarity", 'a', 0, G_OPTION_ARG_DOUBLE,
          &min_avg_similarity,
          "The minimum average similarity under which we consider"
          "the test as failing",
        NULL},
    {"min-lowest-similarity", 'l', 0, G_OPTION_ARG_DOUBLE,
          &min_lowest_similarity,
          "The minimum 'lowest' similarity under which we consider"
          "the test as failing",
        NULL},
    {"result-output-folder", 'r', 0, G_OPTION_ARG_STRING,
          &outfolder,
          "The folder in which to store resulting grey scale images"
          " when the test failed. In that folder you will find"
          " images with the structural difference between"
          " the reference frame and the failed one",
        NULL},
    {NULL}
  };

  g_set_prgname ("gst-validate-mages-check-" GST_API_VERSION);
  ctx = g_option_context_new ("/reference/file/path /compared/file/path");

  g_option_context_set_summary (ctx,
      "The gst-validate-images-check calculates SSIM (Structural SIMilarity) "
      " index for the images. And according to min-lowest-similarity and"
      " min-avg-similarity, it will concider the images similare enough"
      " or report critical issues in the GstValidate reporting system");
  g_option_context_add_main_entries (ctx, options, NULL);

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    return -1;
  }

  if (argc != 3) {
    gchar *help = g_option_context_get_help (ctx, FALSE, NULL);
    g_printerr ("%s", help);
    g_free (help);
    g_option_context_free (ctx);

    return -1;
  }

  gst_init (&argc, &argv);
  gst_validate_init ();

  runner = gst_validate_runner_new ();
  ssim =
      gst_validate_ssim_new (runner, min_avg_similarity, min_lowest_similarity);

  gst_validate_ssim_compare_image_files (ssim, argv[1], argv[2], &mssim,
      &lowest, &highest, outfolder);

  rep_err = gst_validate_runner_exit (runner, TRUE);
  if (ret == 0) {
    ret = rep_err;
    if (rep_err != 0)
      g_print ("Returning %d as error where found", rep_err);
  }

  g_object_unref (ssim);
  g_object_unref (runner);
  gst_validate_deinit ();

  return ret;
}