/* GStreamer
 * Copyright (C) 2013 Thiago Santos <thiago.sousa.santos@collabora.com>
 *
 * gst-validate-monitor-report.c - Validate report/issues functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include "gst-validate-i18n-lib.h"
#include "gst-validate-internal.h"

#include "gst-validate-report.h"
#include "gst-validate-reporter.h"
#include "gst-validate-monitor.h"

static GstClockTime _gst_validate_report_start_time = 0;
static GstValidateDebugFlags _gst_validate_flags = 0;
static GHashTable *_gst_validate_issues = NULL;

G_DEFINE_BOXED_TYPE (GstValidateReport, gst_validate_report,
    (GBoxedCopyFunc) gst_validate_report_ref,
    (GBoxedFreeFunc) gst_validate_report_unref);

GstValidateIssueId
gst_validate_issue_get_id (GstValidateIssue * issue)
{
  return issue->issue_id;
}

GstValidateIssue *
gst_validate_issue_new (GstValidateIssueId issue_id, gchar * summary,
    gchar * description, GstValidateReportLevel default_level)
{
  GstValidateIssue *issue = g_slice_new (GstValidateIssue);

  issue->issue_id = issue_id;
  issue->summary = summary;
  issue->description = description;
  issue->default_level = default_level;
  issue->repeat = FALSE;

  return issue;
}

static void
gst_validate_issue_free (GstValidateIssue * issue)
{
  g_free (issue->summary);
  g_free (issue->description);
  g_slice_free (GstValidateIssue, issue);
}

void
gst_validate_issue_register (GstValidateIssue * issue)
{
  g_return_if_fail (g_hash_table_lookup (_gst_validate_issues,
          (gpointer) gst_validate_issue_get_id (issue)) == NULL);

  g_hash_table_insert (_gst_validate_issues,
      (gpointer) gst_validate_issue_get_id (issue), issue);
}

#define REGISTER_VALIDATE_ISSUE(id,sum,desc,lvl) gst_validate_issue_register (gst_validate_issue_new (id, sum, desc, lvl))
static void
gst_validate_report_load_issues (void)
{
  g_return_if_fail (_gst_validate_issues == NULL);

  _gst_validate_issues = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gst_validate_issue_free);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_BUFFER_BEFORE_SEGMENT,
      _("buffer was received before a segment"),
      _("in push mode, a segment event must be received before a buffer"),
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_BUFFER_IS_OUT_OF_SEGMENT,
      _("buffer is out of the segment range"),
      _("buffer being pushed is out of the current segment's start-stop "
          " range. Meaning it is going to be discarded downstream without "
          "any use"), GST_VALIDATE_REPORT_LEVEL_ISSUE);
  REGISTER_VALIDATE_ISSUE
      (GST_VALIDATE_ISSUE_ID_BUFFER_TIMESTAMP_OUT_OF_RECEIVED_RANGE,
      _("buffer timestamp is out of the received buffer timestamps' range"),
      _("a buffer leaving an element should have its timestamps in the range "
          "of the received buffers timestamps. i.e. If an element received "
          "buffers with timestamps from 0s to 10s, it can't push a buffer with "
          "with a 11s timestamp, because it doesn't have data for that"),
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE
      (GST_VALIDATE_ISSUE_ID_FIRST_BUFFER_RUNNING_TIME_IS_NOT_ZERO,
      _("first buffer's running time isn't 0"),
      _("the first buffer's received running time is expected to be 0"),
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_WRONG_FLOW_RETURN, _("flow return from pad push doesn't match expected value"), _("flow return from a 1:1 sink/src pad element is as simple as " "returning what downstream returned. For elements that have multiple " "src pads, flow returns should be properly combined"), /* TODO fill me more */
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_CAPS_IS_MISSING_FIELD,
      _("caps is missing a required field for its type"),
      _("some caps types are expected to contain a set of basic fields. "
          "For example, raw video should have 'width', 'height', 'framerate' "
          "and 'pixel-aspect-ratio'"), GST_VALIDATE_REPORT_LEVEL_ISSUE);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_CAPS_FIELD_HAS_BAD_TYPE,
      _("caps field has an unexpected type"),
      _("some common caps fields should always use the same expected types"),
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_CAPS_EXPECTED_FIELD_NOT_FOUND,
      _("caps expected field wasn't present"),
      _("a field that should be present in the caps wasn't found. "
          "Fields sets on a sink pad caps should be propagated downstream "
          "when it makes sense to do so"), GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_GET_CAPS_NOT_PROXYING_FIELDS,
      _("getcaps function isn't proxying downstream fields correctly"),
      _("elements should set downstream caps restrictions on its caps when "
          "replying upstream's getcaps queries to avoid upstream sending data"
          " in an unsupported format"), GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_CAPS_FIELD_UNEXPECTED_VALUE,
      _("a field in caps has an unexpected value"),
      _("fields set on a sink pad should be propagated downstream via "
          "set caps"), GST_VALIDATE_REPORT_LEVEL_CRITICAL);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_NEWSEGMENT_NOT_PUSHED,
      _("new segment event wasn't propagated downstream"),
      _("segments received from upstream should be pushed downstream"),
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE
      (GST_VALIDATE_ISSUE_ID_SERIALIZED_EVENT_WASNT_PUSHED_IN_TIME,
      _("a serialized event received should be pushed in the same 'time' "
          "as it was received"),
      _("serialized events should be pushed in the same order they are "
          "received and serialized with buffers. If an event is received after"
          " a buffer with timestamp end 'X', it should be pushed right after "
          "buffers with timestamp end 'X'"), GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_HAS_WRONG_SEQNUM,
      _("events that are part of the same pipeline 'operation' should "
          "have the same seqnum"),
      _("when events/messages are created from another event/message, "
          "they should have their seqnums set to the original event/message "
          "seqnum"), GST_VALIDATE_REPORT_LEVEL_ISSUE);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_SERIALIZED_OUT_OF_ORDER,
      _("a serialized event received should be pushed in the same order "
          "as it was received"),
      _("serialized events should be pushed in the same order they are "
          "received."), GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_NEW_SEGMENT_MISMATCH,
      _("a new segment event has different value than the received one"),
      _("when receiving a new segment, an element should push an equivalent"
          "segment downstream"), GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_FLUSH_START_UNEXPECTED,
      _("received an unexpected flush start event"), NULL,
      GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_FLUSH_STOP_UNEXPECTED,
      _("received an unexpected flush stop event"), NULL,
      GST_VALIDATE_REPORT_LEVEL_WARNING);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_EVENT_SEEK_NOT_HANDLED,
      _("seek event wasn't handled"), NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE
      (GST_VALIDATE_ISSUE_ID_EVENT_SEEK_RESULT_POSITION_WRONG,
      _("position after a seek is wrong"), NULL,
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_STATE_CHANGE_FAILURE,
      _("state change failed"), NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_SIZE_IS_ZERO,
      _("resulting file size is 0"), NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_SIZE_INCORRECT,
      _("resulting file size wasn't within the expected values"),
      NULL, GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_DURATION_INCORRECT,
      _("resulting file duration wasn't within the expected values"),
      NULL, GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_SEEKABLE_INCORRECT,
      _("resulting file wasn't seekable or not seekable as expected"),
      NULL, GST_VALIDATE_REPORT_LEVEL_WARNING);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_PROFILE_INCORRECT,
      _("resulting file stream profiles didn't match expected values"),
      NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_NOT_FOUND,
      _("resulting file could not be found for testing"), NULL,
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_CHECK_FAILURE,
      _("an error occured while checking the file for conformance"), NULL,
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_PLAYBACK_START_FAILURE,
      _("an error occured while starting playback of the test file"), NULL,
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_FILE_PLAYBACK_ERROR,
      _("an error during playback of the file"), NULL,
      GST_VALIDATE_REPORT_LEVEL_CRITICAL);

  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_ALLOCATION_FAILURE,
      _("a memory allocation failed during Validate run"),
      NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);
  REGISTER_VALIDATE_ISSUE (GST_VALIDATE_ISSUE_ID_MISSING_PLUGIN,
      _("a gstreamer plugin is missing and prevented Validate from running"),
      NULL, GST_VALIDATE_REPORT_LEVEL_CRITICAL);
}

void
gst_validate_report_init (void)
{
  const gchar *var;
  const GDebugKey keys[] = {
    {"fatal_criticals", GST_VALIDATE_FATAL_CRITICALS},
    {"fatal_warnings", GST_VALIDATE_FATAL_WARNINGS},
    {"fatal_issues", GST_VALIDATE_FATAL_ISSUES}
  };

  if (_gst_validate_report_start_time == 0) {
    _gst_validate_report_start_time = gst_util_get_timestamp ();

    /* init the debug flags */
    var = g_getenv ("GST_VALIDATE");
    if (var && strlen (var) > 0) {
      _gst_validate_flags = g_parse_debug_string (var, keys, 3);
    }

    gst_validate_report_load_issues ();
  }
}

GstValidateIssue *
gst_validate_issue_from_id (GstValidateIssueId issue_id)
{
  return g_hash_table_lookup (_gst_validate_issues, (gpointer) issue_id);
}

/* TODO how are these functions going to work with extensions */
const gchar *
gst_validate_report_level_get_name (GstValidateReportLevel level)
{
  switch (level) {
    case GST_VALIDATE_REPORT_LEVEL_CRITICAL:
      return "critical";
    case GST_VALIDATE_REPORT_LEVEL_WARNING:
      return "warning";
    case GST_VALIDATE_REPORT_LEVEL_ISSUE:
      return "issue";
    case GST_VALIDATE_REPORT_LEVEL_IGNORE:
      return "ignore";
    default:
      return "unknown";
  }
}

const gchar *
gst_validate_report_area_get_name (GstValidateReportArea area)
{
  switch (area) {
    case GST_VALIDATE_AREA_EVENT:
      return "event";
    case GST_VALIDATE_AREA_BUFFER:
      return "buffer";
    case GST_VALIDATE_AREA_QUERY:
      return "query";
    case GST_VALIDATE_AREA_CAPS:
      return "caps";
    case GST_VALIDATE_AREA_SEEK:
      return "seek";
    case GST_VALIDATE_AREA_STATE:
      return "state";
    case GST_VALIDATE_AREA_FILE_CHECK:
      return "file-check";
    case GST_VALIDATE_AREA_RUN_ERROR:
      return "run-error";
    case GST_VALIDATE_AREA_OTHER:
      return "other";
    default:
      g_assert_not_reached ();
      return "unknown";
  }
}

void
gst_validate_report_check_abort (GstValidateReport * report)
{
  if ((report->level == GST_VALIDATE_REPORT_LEVEL_ISSUE &&
          _gst_validate_flags & GST_VALIDATE_FATAL_ISSUES) ||
      (report->level == GST_VALIDATE_REPORT_LEVEL_WARNING &&
          _gst_validate_flags & GST_VALIDATE_FATAL_WARNINGS) ||
      (report->level == GST_VALIDATE_REPORT_LEVEL_CRITICAL &&
          _gst_validate_flags & GST_VALIDATE_FATAL_CRITICALS)) {
    g_error ("Fatal report received: %" GST_VALIDATE_ERROR_REPORT_PRINT_FORMAT,
        GST_VALIDATE_REPORT_PRINT_ARGS (report));
  }
}

GstValidateIssueId
gst_validate_report_get_issue_id (GstValidateReport * report)
{
  return gst_validate_issue_get_id (report->issue);
}

GstValidateReport *
gst_validate_report_new (GstValidateIssue * issue,
    GstValidateReporter * reporter, const gchar * message)
{
  GstValidateReport *report = g_slice_new0 (GstValidateReport);

  report->issue = issue;
  report->reporter = reporter;  /* TODO should we ref? */
  report->message = g_strdup (message);
  report->timestamp =
      gst_util_get_timestamp () - _gst_validate_report_start_time;
  report->level = issue->default_level;

  return report;
}

void
gst_validate_report_unref (GstValidateReport * report)
{
  if (G_UNLIKELY (g_atomic_int_dec_and_test (&report->refcount))) {
    g_free (report->message);
    g_slice_free (GstValidateReport, report);
  }
}

GstValidateReport *
gst_validate_report_ref (GstValidateReport * report)
{
  g_atomic_int_inc (&report->refcount);

  return report;
}

void
gst_validate_report_printf (GstValidateReport * report)
{
  g_print ("%" GST_VALIDATE_ERROR_REPORT_PRINT_FORMAT "\n",
      GST_VALIDATE_REPORT_PRINT_ARGS (report));
}
