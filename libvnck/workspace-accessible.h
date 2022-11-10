/* vim: set sw=2 et: */
/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VNCK_WORKSPACE_ACCESSIBLE_H__
#define __VNCK_WORKSPACE_ACCESSIBLE_H__

#include <gtk/gtk.h>
#include <atk/atk.h>
#include "workspace.h"
#include "pager-accessible.h"

G_BEGIN_DECLS

#define VNCK_WORKSPACE_TYPE_ACCESSIBLE                     (vnck_workspace_accessible_get_type ())
#define VNCK_WORKSPACE_ACCESSIBLE(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), VNCK_WORKSPACE_TYPE_ACCESSIBLE, WnckWorkspaceAccessible))
#define VNCK_WORKSPACE_ACCESSIBLE_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_WORKSPACE_TYPE_ACCESSIBLE, WnckWorkspaceAccessibleClass))
#define VNCK_IS_WORKSPACE_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VNCK_WORKSPACE_TYPE_ACCESSIBLE))
#define VNCK_IS_WORKSPACE_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), WnckWorkspaceAccessible))
#define VNCK_WORKSPACE_ACCESSIBLE_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_WORKSPACE_TYPE_ACCESSIBLE, WnckWorkspaceAccessibleClass)) 

typedef struct _WnckWorkspaceAccessible WnckWorkspaceAccessible;
typedef struct _WnckWorkspaceAccessibleClass WnckWorkspaceAccessibleClass;

struct _WnckWorkspaceAccessible
{
  AtkGObjectAccessible parent;

  int index;
};

struct _WnckWorkspaceAccessibleClass 
{
  AtkGObjectAccessibleClass parent_class;
};

GType vnck_workspace_accessible_get_type (void) G_GNUC_CONST;

AtkObject* vnck_workspace_accessible_new (GObject *obj); 

G_END_DECLS

#endif /* __VNCK_WORKSPACE_ACCESSIBLE_H__ */
