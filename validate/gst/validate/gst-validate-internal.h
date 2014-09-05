/* GStreamer
 * Copyright (C) 2013 Thiago Santos <thiago.sousa.santos@collabora.com>
 *
 * validate.c - Validate generic functions
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

#ifndef __GST_VALIDATE_INTERNAL_H__
#define __GST_VALIDATE_INTERNAL_H__

#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN (gstvalidate_debug);
#define GST_CAT_DEFAULT gstvalidate_debug

extern GRegex *newline_regex;


typedef struct _GstValidateScenario        GstValidateScenario;
typedef struct _GstValidateAction          GstValidateAction;
typedef struct _GstValidateActionParameter GstValidateActionParameter;
typedef struct _GstValidateActionType      GstValidateActionType;
typedef gboolean (*GstValidateExecuteAction) (GstValidateScenario * scenario, GstValidateAction * action);

struct _GstValidateActionType
{
  GstMiniObject          mini_object;

  gchar *name;
  gchar *implementer_namespace;

  GstValidateExecuteAction execute;

  GstValidateActionParameter *parameters;

  gchar *description;
  gboolean is_config;

  gpointer _gst_reserved[GST_PADDING_LARGE];
};


GST_EXPORT GType _gst_validate_action_type_type;

void init_scenarios (void);

#endif
