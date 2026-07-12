/* class group object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2003 Ximian, Inc.
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 * Copyright (C) 2006-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBVNCK_H_INSIDE__) && !defined (VNCK_COMPILATION)
#error "Only <libvnck/libvnck.h> can be included directly."
#endif

#ifndef VNCK_CLASS_GROUP_H
#define VNCK_CLASS_GROUP_H

#include <glib-object.h>
#include <cdk-pixbuf/cdk-pixbuf.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_TYPE_CLASS_GROUP              (vnck_class_group_get_type ())
#define VNCK_CLASS_GROUP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_CLASS_GROUP, VnckClassGroup))
#define VNCK_CLASS_GROUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_CLASS_GROUP, VnckClassGroupClass))
#define VNCK_IS_CLASS_GROUP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_CLASS_GROUP))
#define VNCK_IS_CLASS_GROUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_CLASS_GROUP))
#define VNCK_CLASS_GROUP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_CLASS_GROUP, VnckClassGroupClass))

typedef struct _VnckClassGroupClass   VnckClassGroupClass;
typedef struct _VnckClassGroupPrivate VnckClassGroupPrivate;

/**
 * VnckClassGroup:
 *
 * The #VnckClassGroup struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckClassGroup
{
  GObject parent_instance;

  VnckClassGroupPrivate *priv;
};

struct _VnckClassGroupClass
{
  GObjectClass parent_class;

  void (* name_changed) (VnckClassGroup *group);
  void (* icon_changed) (VnckClassGroup *group);

  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType vnck_class_group_get_type (void) G_GNUC_CONST;

VnckClassGroup *vnck_class_group_get (const char *id);

GList *vnck_class_group_get_windows (VnckClassGroup *class_group);
const char * vnck_class_group_get_id (VnckClassGroup *class_group);

const char * vnck_class_group_get_name (VnckClassGroup *class_group);

CdkPixbuf *vnck_class_group_get_icon (VnckClassGroup *class_group);
CdkPixbuf *vnck_class_group_get_mini_icon (VnckClassGroup *class_group);

#ifndef VNCK_DISABLE_DEPRECATED
G_DEPRECATED_FOR(vnck_class_group_get_id)
const char * vnck_class_group_get_res_class (VnckClassGroup *class_group);
#endif

G_END_DECLS

#endif /* VNCK_CLASS_GROUP_H */
