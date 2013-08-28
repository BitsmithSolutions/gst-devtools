/* GStreamer
 *
 * Copyright (C) 2013 Collabora Ltd.
 *  Author: Thiago Sousa Santos <thiago.sousa.santos@collabora.com>
 *
 * gst-validate-monitor-factory.c - Validate Element monitors factory utility functions
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

#include "gst-validate-monitor-factory.h"
#include "gst-validate-bin-monitor.h"
#include "gst-validate-pad-monitor.h"
#include "gst-validate-override-registry.h"

GstValidateMonitor *
gst_validate_monitor_factory_create (GstObject * target,
    GstValidateRunner * runner, GstValidateMonitor * parent)
{
  GstValidateMonitor *monitor = NULL;
  g_return_val_if_fail (target != NULL, NULL);

  if (GST_IS_PAD (target)) {
    monitor =
        GST_VALIDATE_MONITOR_CAST (gst_validate_pad_monitor_new (GST_PAD_CAST
            (target), runner, GST_VALIDATE_ELEMENT_MONITOR_CAST (parent)));
  } else if (GST_IS_BIN (target)) {
    monitor =
        GST_VALIDATE_MONITOR_CAST (gst_validate_bin_monitor_new (GST_BIN_CAST
            (target), runner, parent));
  } else if (GST_IS_ELEMENT (target)) {
    monitor =
        GST_VALIDATE_MONITOR_CAST (gst_validate_element_monitor_new
        (GST_ELEMENT_CAST (target), runner, parent));
  }

  g_return_val_if_fail (target != NULL, NULL);
  gst_validate_override_registry_attach_overrides (monitor);
  return monitor;
}
