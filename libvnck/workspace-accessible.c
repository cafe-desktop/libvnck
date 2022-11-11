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

#include <libvnck/libvnck.h>
#include <ctk/ctk.h>
#include <errno.h>
#include <unistd.h>
#include "workspace-accessible.h"
#include "private.h"

static const char* vnck_workspace_accessible_get_name            (AtkObject                    *obj);
static const char* vnck_workspace_accessible_get_description     (AtkObject                    *obj);
static int         vnck_workspace_accessible_get_index_in_parent (AtkObject                    *obj);
static void        atk_component_interface_init                  (AtkComponentIface            *iface);
static void        vnck_workspace_accessible_get_extents         (AtkComponent                 *component,
                                                                  int                          *x,
                                                                  int                          *y,
                                                                  int                          *width,
                                                                  int                          *height,
                                                                  AtkCoordType                  coords);
static void        vnck_workspace_accessible_get_position        (AtkComponent                 *component,
                                                                  int                          *x,
                                                                  int                          *y,
                                                                  AtkCoordType                  coords);
static gboolean    vnck_workspace_accessible_contains            (AtkComponent                 *component,
                                                                  int                           x,
                                                                  int                           y,
                                                                  AtkCoordType                  coords);
static void        vnck_workspace_accessible_get_size            (AtkComponent                 *component,
                                                                  int                          *width,
                                                                  int                          *height);

G_DEFINE_TYPE_WITH_CODE (VnckWorkspaceAccessible,
                         vnck_workspace_accessible,
                         ATK_TYPE_GOBJECT_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT,
                                                atk_component_interface_init))

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_extents = vnck_workspace_accessible_get_extents;
  iface->get_size = vnck_workspace_accessible_get_size;
  iface->get_position = vnck_workspace_accessible_get_position;
  iface->contains = vnck_workspace_accessible_contains;
}

static void
vnck_workspace_accessible_get_extents (AtkComponent *component,
                                       int          *x,
                                       int          *y,
                                       int          *width,
                                       int          *height,
                                       AtkCoordType  coords)
{
  AtkGObjectAccessible *atk_gobj;
  VnckPager *pager;
  GdkRectangle rect;
  GtkWidget *widget;
  AtkObject *parent;
  GObject *g_obj;
  int px, py;

  g_return_if_fail (VNCK_IS_WORKSPACE_ACCESSIBLE (component));

  atk_gobj = ATK_GOBJECT_ACCESSIBLE (component);
  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (g_obj == NULL)
    return;

  g_return_if_fail (VNCK_IS_WORKSPACE (g_obj));

  parent = atk_object_get_parent (ATK_OBJECT(component));
  widget = ctk_accessible_get_widget (CTK_ACCESSIBLE (parent));

  if (widget == NULL)
    {
      /*
       *State is defunct
       */
      return;
    }

  g_return_if_fail (VNCK_IS_PAGER (widget));
  pager = VNCK_PAGER (widget);

  g_return_if_fail (VNCK_IS_PAGER (pager));

  atk_component_get_extents (ATK_COMPONENT (parent), &px, &py, NULL, NULL, coords);

  _vnck_pager_get_workspace_rect (pager, VNCK_WORKSPACE_ACCESSIBLE (component)->index, &rect);

  *x = rect.x + px;
  *y = rect.y + py;
  *height = rect.height;
  *width = rect.width;
}

static void
vnck_workspace_accessible_get_size (AtkComponent *component,
                                    int          *width,
                                    int          *height)
{
  AtkCoordType coords = ATK_XY_SCREEN;
  int x, y;

  /* FIXME: Value for initialization of coords picked randomly to please gcc */

  vnck_workspace_accessible_get_extents (component, &x, &y, width, height, coords);
}

static void
vnck_workspace_accessible_get_position (AtkComponent *component,
		                     int         *x,
			      	     int         *y,
			     	     AtkCoordType  coords)
{
  int width, height;
  vnck_workspace_accessible_get_extents (component, x, y, &width, &height, coords);
}

static gboolean
vnck_workspace_accessible_contains (AtkComponent *component,
                                    int           x,
                                    int           y,
                                    AtkCoordType  coords)
{
  int lx, ly, width, height;

  vnck_workspace_accessible_get_extents (component, &lx, &ly, &width, &height, coords);

  /*
   * Check if the specified co-ordinates fall within the workspace.
   */
  if ( (x > lx) && ((lx + width) >= x)  && (y > ly) && ((ly + height) >= ly) )
    return TRUE;
  else
    return FALSE;
}

static void
vnck_workspace_accessible_class_init (VnckWorkspaceAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_name = vnck_workspace_accessible_get_name;
  class->get_description = vnck_workspace_accessible_get_description;
  class->get_index_in_parent = vnck_workspace_accessible_get_index_in_parent;
}

static void
vnck_workspace_accessible_init (VnckWorkspaceAccessible *accessible)
{
}

AtkObject*
vnck_workspace_accessible_new (GObject *obj)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (VNCK_IS_WORKSPACE (obj), NULL);

  object = g_object_new (VNCK_WORKSPACE_TYPE_ACCESSIBLE, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);

  g_return_val_if_fail (ATK_IS_OBJECT (atk_object), NULL);

  VNCK_WORKSPACE_ACCESSIBLE (atk_object)->index =
    vnck_workspace_get_number (VNCK_WORKSPACE (obj));

  return atk_object;
}

static const char*
vnck_workspace_accessible_get_name (AtkObject *obj)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE_ACCESSIBLE (obj), NULL);

  if (obj->name != NULL)
    {
      return obj->name;
    }
  else
    return NULL;
}

static const char*
vnck_workspace_accessible_get_description (AtkObject *obj)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE_ACCESSIBLE (obj), NULL);

  if (obj->description != NULL)
    {
      return obj->description;
    }
  else
    return NULL;
}

static gint
vnck_workspace_accessible_get_index_in_parent (AtkObject *obj)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE_ACCESSIBLE (obj), -1);

  return VNCK_WORKSPACE_ACCESSIBLE (obj)->index;
}
