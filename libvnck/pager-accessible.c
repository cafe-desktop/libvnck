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

#include <config.h>

#include <libvnck/libvnck.h>
#include <glib/gi18n-lib.h>
#include <ctk/ctk.h>
#include <string.h>
#include <atk/atk.h>
#include "pager-accessible.h"
#include "pager-accessible-factory.h"
#include "workspace-accessible.h"
#include "private.h"

typedef struct _VnckPagerAccessiblePrivate VnckPagerAccessiblePrivate;
struct _VnckPagerAccessiblePrivate
{
  GSList *children;
};

static const char* vnck_pager_accessible_get_name         (AtkObject                *obj);
static const char* vnck_pager_accessible_get_description  (AtkObject                *obj);
static int         vnck_pager_accessible_get_n_children   (AtkObject                *obj);
static AtkObject*  vnck_pager_accessible_ref_child        (AtkObject                *obj,
                                                           int                       i);
static void        atk_selection_interface_init           (AtkSelectionIface        *iface);
static gboolean    vnck_pager_add_selection               (AtkSelection             *selection,
                                                           int                       i);
static gboolean    vnck_pager_is_child_selected           (AtkSelection             *selection,
                                                           int                       i);
static AtkObject*  vnck_pager_ref_selection               (AtkSelection             *selection,
                                                           int                       i);
static int         vnck_pager_selection_count             (AtkSelection             *selection);
static void        vnck_pager_accessible_update_workspace (AtkObject                *aobj_ws,
                                                           VnckPager                *pager,
                                                           int                       i);
static void        vnck_pager_accessible_finalize         (GObject                  *gobject);

G_DEFINE_TYPE_WITH_CODE (VnckPagerAccessible,
                         vnck_pager_accessible,
                         CTK_TYPE_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION,
                                                atk_selection_interface_init)
                         G_ADD_PRIVATE (VnckPagerAccessible))

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->add_selection = vnck_pager_add_selection;
  iface->ref_selection = vnck_pager_ref_selection;
  iface->get_selection_count = vnck_pager_selection_count;
  iface->is_child_selected = vnck_pager_is_child_selected;
}

static void
vnck_pager_accessible_class_init (VnckPagerAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  class->get_name = vnck_pager_accessible_get_name;
  class->get_description = vnck_pager_accessible_get_description;
  class->get_n_children = vnck_pager_accessible_get_n_children;
  class->ref_child = vnck_pager_accessible_ref_child;

  obj_class->finalize = vnck_pager_accessible_finalize;
}

static void
vnck_pager_accessible_init (VnckPagerAccessible *accessible G_GNUC_UNUSED)
{
}

static gboolean
vnck_pager_add_selection (AtkSelection *selection,
		          int           i)
{
  VnckPager *pager;
  VnckWorkspace *wspace;
  CtkWidget *widget;
  int n_spaces;

  widget = ctk_accessible_get_widget (CTK_ACCESSIBLE (selection));

  if (widget == NULL)
    {
      /*
       *State is defunct
       */
      return FALSE;
    }

  pager = VNCK_PAGER (widget);
  n_spaces = _vnck_pager_get_n_workspaces (pager);

  if (i < 0 || i >= n_spaces)
    return FALSE;

  /*
   * Activate the following worksapce as current workspace
   */
  wspace = _vnck_pager_get_workspace (pager, i);
  /* FIXME: Is ctk_get_current_event_time() good enough here?  I have no idea */
  _vnck_pager_activate_workspace (wspace, ctk_get_current_event_time ());

  return TRUE;
}

/*
 * Returns the AtkObject of the selected WorkSpace
 */
static AtkObject*
vnck_pager_ref_selection (AtkSelection *selection,
		          int           i)
{
  VnckPager *pager;
  CtkWidget *widget;
  VnckWorkspace *active_wspace;
  AtkObject *accessible;
  int wsno;

  g_return_val_if_fail (i == 0, NULL);

  widget = ctk_accessible_get_widget (CTK_ACCESSIBLE (selection));

  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return NULL;
    }
  pager = VNCK_PAGER (widget);

  active_wspace = VNCK_WORKSPACE (_vnck_pager_get_active_workspace (pager));
  wsno = vnck_workspace_get_number (active_wspace);

  accessible = ATK_OBJECT (vnck_pager_accessible_ref_child (ATK_OBJECT (selection), wsno));

  return accessible;
}

/*
 * Returns the no.of child selected, it should be either 1 or 0
 * b'coz only one child can be selected at a time.
 */
static int
vnck_pager_selection_count (AtkSelection *selection)
{
  CtkWidget *widget;

  widget = ctk_accessible_get_widget (CTK_ACCESSIBLE (selection));

  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return 0;
    }
  else
    {
      return 1;
    }
}

/*
 *Checks whether the WorkSpace specified by i is selected,
 *and returns TRUE on selection.
 */
static gboolean
vnck_pager_is_child_selected (AtkSelection *selection,
		              int           i)
{
  VnckPager *pager;
  CtkWidget *widget;
  VnckWorkspace *active_wspace;
  int wsno;

  widget = ctk_accessible_get_widget (CTK_ACCESSIBLE (selection));

  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return FALSE;
    }

  pager = VNCK_PAGER (widget);
  active_wspace = _vnck_pager_get_active_workspace (pager);

  wsno = vnck_workspace_get_number (active_wspace);

  return (wsno == i);
}

AtkObject*
vnck_pager_accessible_new (CtkWidget *widget)
{
  GObject *object;
  AtkObject *aobj_pager;
  CtkAccessible *ctk_accessible;

  object = g_object_new (VNCK_PAGER_TYPE_ACCESSIBLE, NULL);

  aobj_pager = ATK_OBJECT (object);

  ctk_accessible = CTK_ACCESSIBLE (aobj_pager);
  ctk_accessible_set_widget (ctk_accessible, widget);

  atk_object_initialize (aobj_pager, widget);
  aobj_pager->role = ATK_ROLE_PANEL;

  return aobj_pager;
}

static void
vnck_pager_accessible_finalize (GObject *gobject)
{
  VnckPagerAccessible *accessible;
  VnckPagerAccessiblePrivate *priv;

  accessible = VNCK_PAGER_ACCESSIBLE (gobject);
  priv = vnck_pager_accessible_get_instance_private (accessible);

  if (priv)
    {
      if (priv->children)
        {
          g_slist_free_full (priv->children, g_object_unref);
          priv->children = NULL;
        }
    }

  G_OBJECT_CLASS (vnck_pager_accessible_parent_class)->finalize (gobject);
}

static const char*
vnck_pager_accessible_get_name (AtkObject *obj)
{
  g_return_val_if_fail (VNCK_PAGER_IS_ACCESSIBLE (obj), NULL);

  if (obj->name == NULL)
    obj->name = g_strdup (_("Workspace Switcher"));

  return obj->name;
}

static const char*
vnck_pager_accessible_get_description (AtkObject *obj)
{
  g_return_val_if_fail (VNCK_PAGER_IS_ACCESSIBLE (obj), NULL);

  if (obj->description == NULL)
    obj->description = g_strdup (_("Tool to switch between workspaces"));

  return obj->description;
}

/*
 * Number of workspaces is returned as n_children
 */
static int
vnck_pager_accessible_get_n_children (AtkObject* obj)
{
  CtkAccessible *accessible;
  CtkWidget *widget;
  VnckPager *pager;

  g_return_val_if_fail (VNCK_PAGER_IS_ACCESSIBLE (obj), 0);

  accessible = CTK_ACCESSIBLE (obj);
  widget = ctk_accessible_get_widget (accessible);

  if (widget == NULL)
    /* State is defunct */
    return 0;

  pager = VNCK_PAGER (widget);

  return _vnck_pager_get_n_workspaces (pager);
}

/*
 * Will return appropriate static AtkObject for the workspaces
 */
static AtkObject*
vnck_pager_accessible_ref_child (AtkObject *obj,
                                 int        i)
{
  CtkAccessible *accessible;
  CtkWidget *widget;
  VnckPager *pager;
  int n_spaces = 0;
  int len;
  VnckPagerAccessible *pager_accessible;
  VnckPagerAccessiblePrivate *priv;
  AtkObject *ret;

  g_return_val_if_fail (VNCK_PAGER_IS_ACCESSIBLE (obj), NULL);
  g_return_val_if_fail (ATK_IS_OBJECT (obj), NULL);

  accessible = CTK_ACCESSIBLE (obj);
  widget = ctk_accessible_get_widget (accessible);

  if (widget == NULL)
    /* State is defunct */
    return NULL;

  pager = VNCK_PAGER (widget);
  pager_accessible = VNCK_PAGER_ACCESSIBLE (obj);
  priv = vnck_pager_accessible_get_instance_private (pager_accessible);

  len = g_slist_length (priv->children);
  n_spaces = _vnck_pager_get_n_workspaces (pager);

  if (i < 0 || i >= n_spaces)
    return NULL;

  /* We are really inefficient about this due to all the appending,
   * and never shrink the list either.
   */
  while (n_spaces > len)
    {
      AtkRegistry *default_registry;
      AtkObjectFactory *factory;
      VnckWorkspace *wspace;
      VnckWorkspaceAccessible *space_accessible;

      default_registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (default_registry,
                                          VNCK_TYPE_WORKSPACE);

      wspace = _vnck_pager_get_workspace (pager, len);
      space_accessible = VNCK_WORKSPACE_ACCESSIBLE (atk_object_factory_create_accessible (factory,
                                                                                          G_OBJECT (wspace)));
      atk_object_set_parent (ATK_OBJECT (space_accessible), obj);

      priv->children = g_slist_append (priv->children, space_accessible);

      ++len;
    }

  ret = g_slist_nth_data (priv->children, i);
  g_object_ref (G_OBJECT (ret));
  vnck_pager_accessible_update_workspace (ret, pager, i);

  return ret;
}

static void
vnck_pager_accessible_update_workspace (AtkObject *aobj_ws,
                                        VnckPager *pager,
                                        int        i)
{
  g_free (aobj_ws->name);
  aobj_ws->name = g_strdup (_vnck_pager_get_workspace_name (pager, i));

  g_free (aobj_ws->description);
  aobj_ws->description = g_strdup_printf (_("Click this to switch to workspace %s"),
                                          aobj_ws->name);
  aobj_ws->role = ATK_ROLE_UNKNOWN;
}
