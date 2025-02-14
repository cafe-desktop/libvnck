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

#include <ctk/ctk.h>
#include "workspace-accessible-factory.h"
#include "workspace-accessible.h"

G_DEFINE_TYPE (VnckWorkspaceAccessibleFactory,
               vnck_workspace_accessible_factory, ATK_TYPE_OBJECT_FACTORY);

static AtkObject* vnck_workspace_accessible_factory_create_accessible (GObject *obj);

static GType vnck_workspace_accessible_factory_get_accessible_type (void);

static void
vnck_workspace_accessible_factory_class_init (VnckWorkspaceAccessibleFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = vnck_workspace_accessible_factory_create_accessible;
  class->get_accessible_type = vnck_workspace_accessible_factory_get_accessible_type;
}

static void
vnck_workspace_accessible_factory_init (VnckWorkspaceAccessibleFactory *factory G_GNUC_UNUSED)
{
}

AtkObjectFactory*
vnck_workspace_accessible_factory_new (void)
{
  GObject *factory;
  factory = g_object_new (VNCK_TYPE_WORKSPACE_ACCESSIBLE_FACTORY, NULL);
  return ATK_OBJECT_FACTORY (factory);
}

static AtkObject*
vnck_workspace_accessible_factory_create_accessible (GObject *obj)
{
  return vnck_workspace_accessible_new (obj);
}

static GType
vnck_workspace_accessible_factory_get_accessible_type (void)
{
  return VNCK_WORKSPACE_TYPE_ACCESSIBLE;
}
