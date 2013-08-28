/* GStreamer
 *
 * Copyright (C) 2013 Collabora Ltd.
 *  Author: Thiago Sousa Santos <thiago.sousa.santos@collabora.com>
 *
 * gst-validate-override-registry.c - Validate Override Registry
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

#define __USE_GNU
#include <dlfcn.h>

#include "gst-validate-internal.h"
#include "gst-validate-override-registry.h"

typedef struct
{
  gchar *name;
  GstValidateOverride *override;
} GstValidateOverrideRegistryNameEntry;

typedef struct
{
  GType gtype;
  GstValidateOverride *override;
} GstValidateOverrideRegistryGTypeEntry;

static GMutex _gst_validate_override_registry_mutex;
static GstValidateOverrideRegistry *_registry_default;

#define GST_VALIDATE_OVERRIDE_REGISTRY_LOCK(r) g_mutex_lock (&r->mutex)
#define GST_VALIDATE_OVERRIDE_REGISTRY_UNLOCK(r) g_mutex_unlock (&r->mutex)

#define GST_VALIDATE_OVERRIDE_INIT_SYMBOL "gst_validate_create_overrides"

static GstValidateOverrideRegistry *
gst_validate_override_registry_new (void)
{
  GstValidateOverrideRegistry *reg = g_slice_new0 (GstValidateOverrideRegistry);

  g_mutex_init (&reg->mutex);
  g_queue_init (&reg->name_overrides);
  g_queue_init (&reg->gtype_overrides);
  g_queue_init (&reg->klass_overrides);

  return reg;
}

GstValidateOverrideRegistry *
gst_validate_override_registry_get (void)
{
  g_mutex_lock (&_gst_validate_override_registry_mutex);
  if (G_UNLIKELY (!_registry_default)) {
    _registry_default = gst_validate_override_registry_new ();
  }
  g_mutex_unlock (&_gst_validate_override_registry_mutex);

  return _registry_default;
}

void
gst_validate_override_register_by_name (const gchar * name,
    GstValidateOverride * override)
{
  GstValidateOverrideRegistry *registry = gst_validate_override_registry_get ();
  GstValidateOverrideRegistryNameEntry *entry =
      g_slice_new (GstValidateOverrideRegistryNameEntry);

  GST_VALIDATE_OVERRIDE_REGISTRY_LOCK (registry);
  entry->name = g_strdup (name);
  entry->override = override;
  g_queue_push_tail (&registry->name_overrides, entry);
  GST_VALIDATE_OVERRIDE_REGISTRY_UNLOCK (registry);
}

void
gst_validate_override_register_by_type (GType gtype,
    GstValidateOverride * override)
{
  GstValidateOverrideRegistry *registry = gst_validate_override_registry_get ();
  GstValidateOverrideRegistryGTypeEntry *entry =
      g_slice_new (GstValidateOverrideRegistryGTypeEntry);

  GST_VALIDATE_OVERRIDE_REGISTRY_LOCK (registry);
  entry->gtype = gtype;
  entry->override = override;
  g_queue_push_tail (&registry->gtype_overrides, entry);
  GST_VALIDATE_OVERRIDE_REGISTRY_UNLOCK (registry);
}

void
gst_validate_override_register_by_klass (const gchar * klass,
    GstValidateOverride * override)
{
  GstValidateOverrideRegistry *registry = gst_validate_override_registry_get ();
  GstValidateOverrideRegistryNameEntry *entry =
      g_slice_new (GstValidateOverrideRegistryNameEntry);

  GST_VALIDATE_OVERRIDE_REGISTRY_LOCK (registry);
  entry->name = g_strdup (klass);
  entry->override = override;
  g_queue_push_tail (&registry->klass_overrides, entry);
  GST_VALIDATE_OVERRIDE_REGISTRY_UNLOCK (registry);
}

static void
    gst_validate_override_registry_attach_name_overrides_unlocked
    (GstValidateOverrideRegistry * registry, GstValidateMonitor * monitor)
{
  GstValidateOverrideRegistryNameEntry *entry;
  GList *iter;
  const gchar *name;

  name = gst_validate_monitor_get_element_name (monitor);
  for (iter = registry->name_overrides.head; iter; iter = g_list_next (iter)) {
    entry = iter->data;
    if (strcmp (name, entry->name) == 0) {
      gst_validate_monitor_attach_override (monitor, entry->override);
    }
  }
}

static void
    gst_validate_override_registry_attach_gtype_overrides_unlocked
    (GstValidateOverrideRegistry * registry, GstValidateMonitor * monitor)
{
  GstValidateOverrideRegistryGTypeEntry *entry;
  GstElement *element;
  GList *iter;

  element = gst_validate_monitor_get_element (monitor);
  if (!element)
    return;

  for (iter = registry->name_overrides.head; iter; iter = g_list_next (iter)) {
    entry = iter->data;
    if (G_TYPE_CHECK_INSTANCE_TYPE (element, entry->gtype)) {
      gst_validate_monitor_attach_override (monitor, entry->override);
    }
  }
}

static void
    gst_validate_override_registry_attach_klass_overrides_unlocked
    (GstValidateOverrideRegistry * registry, GstValidateMonitor * monitor)
{
  GstValidateOverrideRegistryNameEntry *entry;
  GList *iter;
  GstElement *element;
  GstElementClass *klass;

  element = gst_validate_monitor_get_element (monitor);
  if (!element)
    return;

  klass = GST_ELEMENT_GET_CLASS (element);

  for (iter = registry->name_overrides.head; iter; iter = g_list_next (iter)) {
    entry = iter->data;
    /* TODO It would be more correct to split it before comparing */
    if (strstr (klass->details.klass, entry->name) != NULL) {
      gst_validate_monitor_attach_override (monitor, entry->override);
    }
  }
}

void
gst_validate_override_registry_attach_overrides (GstValidateMonitor * monitor)
{
  GstValidateOverrideRegistry *reg = gst_validate_override_registry_get ();

  GST_VALIDATE_OVERRIDE_REGISTRY_LOCK (reg);
  gst_validate_override_registry_attach_name_overrides_unlocked (reg, monitor);
  gst_validate_override_registry_attach_gtype_overrides_unlocked (reg, monitor);
  gst_validate_override_registry_attach_klass_overrides_unlocked (reg, monitor);
  GST_VALIDATE_OVERRIDE_REGISTRY_UNLOCK (reg);
}

int
gst_validate_override_registry_preload (void)
{
  gchar **solist, *const *so;
  const char *sos, *soerr;
  void *sol;
  int ret, (*entry) (void), nloaded = 0;

  sos = g_getenv ("GST_VALIDATE_OVERRIDE");
  if (!sos) {
    GST_INFO ("No GST_VALIDATE_OVERRIDE found, no overrides to load");
    return 0;
  }
  solist = g_strsplit (sos, ",", 0);
  for (so = solist; *so; ++so) {
    GST_INFO ("Loading overrides from %s", *so);
    sol = dlopen (*so, RTLD_LAZY);
    if (!sol) {
      soerr = dlerror ();
      GST_ERROR ("Failed to load %s %s", *so, soerr ? soerr : "no idea why");
      continue;
    }
    entry = dlsym (sol, GST_VALIDATE_OVERRIDE_INIT_SYMBOL);
    if (entry) {
      ret = (*entry) ();
      if (ret > 0) {
        GST_INFO ("Loaded %d overrides from %s", ret, *so);
        nloaded += ret;
      } else if (ret < 0) {
        GST_WARNING ("Error loading overrides from %s", *so);
      } else {
        GST_INFO ("Loaded no overrides from %s", *so);
      }
    } else {
      GST_WARNING (GST_VALIDATE_OVERRIDE_INIT_SYMBOL " not found in %s", *so);
    }
    dlclose (sol);
  }
  g_strfreev (solist);
  GST_INFO ("%d overrides loaded", nloaded);
  return nloaded;
}
