/* selector */

/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2005-2007 Vincent Untz
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
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#include <config.h>

#include <ctk/ctk.h>

#include <glib/gi18n-lib.h>
#include "selector.h"
#include "libvnck.h"
#include "screen.h"
#include "vnck-image-menu-item-private.h"
#include "private.h"

/**
 * SECTION:selector
 * @short_description: a window selector widget, showing the list of windows as
 * a menu.
 * @see_also: #VnckTasklist
 * @stability: Unstable
 *
 * The #VnckSelector represents client windows on a screen as a menu, where
 * menu items are labelled with the window titles and icons. Activating a menu
 * item activates the represented window.
 *
 * The #VnckSelector will automatically detect the screen it is on, and will
 * represent windows of this screen only.
 */

struct _VnckSelectorPrivate {
  CtkWidget  *image;
  VnckWindow *icon_window;

  /* those have the same lifecycle as the menu */
  CtkWidget  *menu;
  CtkWidget  *no_windows_item;
  GHashTable *window_hash;
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckSelector, vnck_selector, CTK_TYPE_MENU_BAR);

static GObject *vnck_selector_constructor (GType                  type,
                                           guint                  n_construct_properties,
                                           GObjectConstructParam *construct_properties);
static void vnck_selector_dispose           (GObject           *object);
static void vnck_selector_finalize          (GObject           *object);
static void vnck_selector_realize           (CtkWidget *widget);
static void vnck_selector_unrealize         (CtkWidget *widget);
static gboolean vnck_selector_scroll_event  (CtkWidget        *widget,
                                             CdkEventScroll   *event);
static void vnck_selector_connect_to_window (VnckSelector      *selector,
                                             VnckWindow        *window);

static void vnck_selector_insert_window (VnckSelector *selector,
                                         VnckWindow   *window);
static void vnck_selector_append_window (VnckSelector *selector,
                                         VnckWindow   *window);

static gint
vnck_selector_windows_compare (gconstpointer  a,
                               gconstpointer  b)
{
  int posa;
  int posb;

  posa = vnck_window_get_sort_order (VNCK_WINDOW (a));
  posb = vnck_window_get_sort_order (VNCK_WINDOW (b));

  return (posa - posb);
}

static void
vncklet_connect_while_alive (gpointer object,
                             const char *signal,
                             GCallback func,
                             gpointer func_data, gpointer alive_object)
{
  GClosure *closure;

  closure = g_cclosure_new (func, func_data, NULL);
  g_object_watch_closure (G_OBJECT (alive_object), closure);
  g_signal_connect_closure_by_id (object,
                                  g_signal_lookup (signal,
                                                   G_OBJECT_TYPE (object)), 0,
                                  closure, FALSE);
}

static VnckScreen *
vnck_selector_get_screen (VnckSelector *selector)
{
  CdkScreen *screen;

  g_assert (ctk_widget_has_screen (CTK_WIDGET (selector)));

  screen = ctk_widget_get_screen (CTK_WIDGET (selector));

  return vnck_screen_get (cdk_x11_screen_get_screen_number (screen));
}

static GdkPixbuf *
vnck_selector_get_default_window_icon (void)
{
  static GdkPixbuf *retval = NULL;

  if (retval)
    return retval;

  retval = gdk_pixbuf_new_from_resource ("/org/gnome/libvnck/default_icon.png", NULL);

  g_assert (retval);

  return retval;
}

static GdkPixbuf *
vnck_selector_dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;
  GdkPixbuf *dimmed;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    dimmed = gdk_pixbuf_copy (pixbuf);
  else
    dimmed = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (dimmed);
  row_stride = gdk_pixbuf_get_rowstride (dimmed);

  for (y = 0; y < h; y++)
    {
      pixels = row;
      for (x = 0; x < w; x++)
        {
          pixels[3] /= 2;
          pixels += pixel_stride;
        }
      row += row_stride;
    }

  return dimmed;
}

void
_vnck_selector_set_window_icon (CtkWidget  *image,
                                VnckWindow *window)
{
  GdkPixbuf *pixbuf, *freeme, *freeme2;
  int width, height;
  int icon_size = -1;

  pixbuf = NULL;
  freeme = NULL;
  freeme2 = NULL;

  if (window)
    pixbuf = vnck_window_get_mini_icon (window);

  if (!pixbuf)
    pixbuf = vnck_selector_get_default_window_icon ();

  if (icon_size == -1)
    ctk_icon_size_lookup (CTK_ICON_SIZE_MENU, NULL, &icon_size);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (icon_size != -1 && (width > icon_size || height > icon_size))
    {
      double scale;

      scale = ((double) icon_size) / MAX (width, height);

      pixbuf = gdk_pixbuf_scale_simple (pixbuf, width * scale,
                                        height * scale, GDK_INTERP_BILINEAR);
      freeme = pixbuf;
    }

  if (window && vnck_window_is_minimized (window))
    {
      pixbuf = vnck_selector_dimm_icon (pixbuf);
      freeme2 = pixbuf;
    }

  ctk_image_set_from_pixbuf (CTK_IMAGE (image), pixbuf);

  if (freeme)
    g_object_unref (freeme);
  if (freeme2)
    g_object_unref (freeme2);
}

static void
vnck_selector_set_active_window (VnckSelector *selector, VnckWindow *window)
{
  _vnck_selector_set_window_icon (selector->priv->image, window);
  selector->priv->icon_window = window;
}

static void
vnck_selector_make_menu_consistent (VnckSelector *selector)
{
  GList     *l, *children;
  int        workspace_n;
  CtkWidget *workspace_item;
  CtkWidget *separator;
  gboolean   separator_is_first;
  gboolean   separator_is_last;
  gboolean   visible_window;

  workspace_n = -1;
  workspace_item = NULL;

  separator = NULL;
  separator_is_first = FALSE;
  separator_is_last = FALSE;

  visible_window = FALSE;

  children = ctk_container_get_children (CTK_CONTAINER (selector->priv->menu));

  for (l = children; l; l = l->next)
    {
      int i;

      i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                                              "vnck-selector-workspace-n"));

      if (i > 0)
        {
          workspace_n = i - 1;

          /* we have two consecutive workspace items => hide the first */
          if (workspace_item)
            ctk_widget_hide (workspace_item);

          workspace_item = CTK_WIDGET (l->data);
        }
      else if (CTK_IS_SEPARATOR_MENU_ITEM (l->data))
        {
          if (!visible_window)
            separator_is_first = TRUE;
          separator_is_last = TRUE;
          separator = CTK_WIDGET (l->data);
        }
      else if (ctk_widget_get_visible (l->data) &&
               l->data != selector->priv->no_windows_item)
        {
          separator_is_last = FALSE;
          visible_window = TRUE;

          /* if we know of a workspace item that was not shown */
          if (workspace_item)
            {
              VnckWindow    *window;
              VnckWorkspace *workspace;

              window = g_object_get_data (G_OBJECT (l->data),
                                          "vnck-selector-window");

              if (window)
                {
                  workspace = vnck_window_get_workspace (window);
                  if (workspace &&
                      workspace_n == vnck_workspace_get_number (workspace))
                    {
                      ctk_widget_show (workspace_item);
                      workspace_n = -1;
                      workspace_item = NULL;
                    }
                }
            }
        } /* end if (normal item) */
    }

  g_list_free (children);

  /* do we have a trailing workspace item to be hidden? */
  if (workspace_item)
    ctk_widget_hide (workspace_item);

  if (separator)
    {
      if (separator_is_first || separator_is_last)
        ctk_widget_hide (separator);
      else
        ctk_widget_show (separator);
    }

  if (visible_window)
    ctk_widget_hide (selector->priv->no_windows_item);
  else
    ctk_widget_show (selector->priv->no_windows_item);
}

static void
vnck_selector_window_icon_changed (VnckWindow *window,
                                   VnckSelector *selector)
{
  CtkWidget *item;

  if (selector->priv->icon_window == window)
    vnck_selector_set_active_window (selector, window);

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item != NULL)
    {
      vnck_image_menu_item_set_image_from_window (VNCK_IMAGE_MENU_ITEM (item),
                                                  window);
    }
}

static void
vnck_selector_window_name_changed (VnckWindow *window,
                                   VnckSelector *selector)
{
  CtkWidget *item;
  char *window_name;

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item != NULL)
    {
      window_name = _vnck_window_get_name_for_display (window, FALSE, TRUE);
      ctk_menu_item_set_label (CTK_MENU_ITEM (item), window_name);
      g_free (window_name);
    }
}

static void
vnck_selector_window_state_changed (VnckWindow     *window,
				    VnckWindowState changed_mask,
				    VnckWindowState new_state G_GNUC_UNUSED,
VnckSelector *selector)
{
  CtkWidget *item;
  char *window_name;

  if (!
      (changed_mask &
       (VNCK_WINDOW_STATE_MINIMIZED | VNCK_WINDOW_STATE_SHADED |
        VNCK_WINDOW_STATE_SKIP_TASKLIST |
        VNCK_WINDOW_STATE_DEMANDS_ATTENTION |
        VNCK_WINDOW_STATE_URGENT)))
    return;

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item == NULL)
    return;

  if (changed_mask & VNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      if (vnck_window_is_skip_tasklist (window))
        ctk_widget_hide (item);
      else
        ctk_widget_show (item);

      vnck_selector_make_menu_consistent (selector);

      ctk_menu_reposition (CTK_MENU (selector->priv->menu));
    }

  if (changed_mask &
      (VNCK_WINDOW_STATE_DEMANDS_ATTENTION | VNCK_WINDOW_STATE_URGENT))
    {
      if (vnck_window_or_transient_needs_attention (window))
        vnck_image_menu_item_make_label_bold (VNCK_IMAGE_MENU_ITEM (item));
      else
        vnck_image_menu_item_make_label_normal (VNCK_IMAGE_MENU_ITEM (item));
    }

  if (changed_mask &
      (VNCK_WINDOW_STATE_MINIMIZED | VNCK_WINDOW_STATE_SHADED))
    {
      window_name = _vnck_window_get_name_for_display (window, FALSE, TRUE);
      ctk_menu_item_set_label (CTK_MENU_ITEM (item), window_name);
      g_free (window_name);
    }
}

static void
vnck_selector_window_workspace_changed (VnckWindow   *window,
                                        VnckSelector *selector)
{
  CtkWidget *item;

  if (!selector->priv->menu || !ctk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (!item)
    return;

  /* destroy the item and recreate one so it's at the right position */
  ctk_widget_destroy (item);
  g_hash_table_remove (selector->priv->window_hash, window);

  vnck_selector_insert_window (selector, window);
  vnck_selector_make_menu_consistent (selector);

  ctk_menu_reposition (CTK_MENU (selector->priv->menu));
}

static void
vnck_selector_active_window_changed (VnckScreen   *screen,
				     VnckWindow   *previous_window G_GNUC_UNUSED,
				     VnckSelector *selector)
{
  VnckWindow *window;

  window = vnck_screen_get_active_window (screen);

  if (selector->priv->icon_window != window)
    vnck_selector_set_active_window (selector, window);
}

static void
vnck_selector_activate_window (VnckWindow *window)
{
  VnckWorkspace *workspace;
  guint32 timestamp;

  /* We're in an activate callback, so ctk_get_current_time() works... */
  timestamp = ctk_get_current_event_time ();

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  workspace = vnck_window_get_workspace (window);
  if (workspace)
    vnck_workspace_activate (workspace, timestamp);

  vnck_window_activate (window, timestamp);
}

static void
vnck_selector_drag_begin (CtkWidget          *widget,
			  CdkDragContext     *context,
			  VnckWindow         *window)
{
  while (widget)
    {
      if (VNCK_IS_SELECTOR (widget))
        break;

      if (CTK_IS_MENU (widget))
        widget = ctk_menu_get_attach_widget (CTK_MENU (widget));
      else
        widget = ctk_widget_get_parent (widget);
    }

  if (widget)
    _vnck_window_set_as_drag_icon (window, context, widget);
}

static void
vnck_selector_drag_data_get (CtkWidget        *widget G_GNUC_UNUSED,
			     CdkDragContext   *context G_GNUC_UNUSED,
			     CtkSelectionData *selection_data,
			     guint             info G_GNUC_UNUSED,
			     guint             time G_GNUC_UNUSED,
			     VnckWindow       *window)
{
  gulong xid;

  xid = vnck_window_get_xid (window);
  ctk_selection_data_set (selection_data,
                          ctk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static CtkWidget *
vnck_selector_item_new (VnckSelector *selector,
                        const gchar *label, VnckWindow *window)
{
  CtkWidget *item;
  static const CtkTargetEntry targets[] = {
    { (gchar *) "application/x-vnck-window-id", 0, 0 }
  };

  item = vnck_image_menu_item_new_with_label (label);

  if (window != NULL)
    {
      /* if window demands attention, bold the label */
      if (vnck_window_or_transient_needs_attention (window))
        vnck_image_menu_item_make_label_bold (VNCK_IMAGE_MENU_ITEM (item));

      g_hash_table_insert (selector->priv->window_hash, window, item);
    }

  if (window != NULL)
    {
      ctk_drag_source_set (item,
                           CDK_BUTTON1_MASK,
                           targets, 1,
                           CDK_ACTION_MOVE);

      g_signal_connect_object (item, "drag_data_get",
                               G_CALLBACK (vnck_selector_drag_data_get),
                               G_OBJECT (window),
                               0);

      g_signal_connect_object (item, "drag_begin",
                               G_CALLBACK (vnck_selector_drag_begin),
                               G_OBJECT (window),
                               0);
    }

  return item;
}

static void
vnck_selector_workspace_name_changed (VnckWorkspace *workspace,
                                      CtkLabel      *label)
{
  CtkStyleContext *context;
  CdkRGBA          color;
  char            *name;
  char            *markup;

  context = ctk_widget_get_style_context (CTK_WIDGET (label));

  ctk_style_context_save (context);
  ctk_style_context_set_state (context, CTK_STATE_FLAG_INSENSITIVE);
  ctk_style_context_get_color (context, CTK_STATE_FLAG_INSENSITIVE, &color);
  ctk_style_context_restore (context);

  name = g_markup_escape_text (vnck_workspace_get_name (workspace), -1);
  markup = g_strdup_printf ("<span size=\"x-small\" style=\"italic\" foreground=\"#%.2x%.2x%.2x\">%s</span>",
                            (int)(color.red * 65535 + 0.5),
                            (int)(color.green * 65535 + 0.5),
                            (int)(color.blue * 65535 + 0.5), name);
  g_free (name);

  ctk_label_set_markup (label, markup);
  g_free (markup);
}

static void
vnck_selector_workspace_label_style_updated (CtkLabel      *label,
                                             VnckWorkspace *workspace)
{
  vnck_selector_workspace_name_changed (workspace, label);
}

static void
vnck_selector_add_workspace (VnckSelector *selector,
                             VnckScreen   *screen,
                             int           workspace_n)
{
  VnckWorkspace *workspace;
  CtkWidget     *item;
  CtkWidget     *label;

  workspace = vnck_screen_get_workspace (screen, workspace_n);

  /* We use a separator in which we add a label. This makes the menu item not
   * selectable without any hack. */
  item = ctk_separator_menu_item_new ();

  label = ctk_label_new ("");
  ctk_label_set_xalign (CTK_LABEL (label), 1.0);
  ctk_widget_show (label);
  /* the handler will also take care of setting the name for the first time,
   * and we'll be able to adapt to theme changes */
  g_signal_connect (G_OBJECT (label), "style-updated",
                    G_CALLBACK (vnck_selector_workspace_label_style_updated),
		    workspace);
  vncklet_connect_while_alive (workspace, "name_changed",
                               G_CALLBACK (vnck_selector_workspace_name_changed),
                                label, label);

  ctk_container_add (CTK_CONTAINER (item), label);

  ctk_menu_shell_append (CTK_MENU_SHELL (selector->priv->menu), item);

  g_object_set_data (G_OBJECT (item), "vnck-selector-workspace-n",
                     GINT_TO_POINTER (workspace_n + 1));
}

static CtkWidget *
vnck_selector_create_window (VnckSelector *selector, VnckWindow *window)
{
  CtkWidget *item;
  char *name;

  name = _vnck_window_get_name_for_display (window, FALSE, TRUE);

  item = vnck_selector_item_new (selector, name, window);
  g_free (name);

  vnck_image_menu_item_set_image_from_window (VNCK_IMAGE_MENU_ITEM (item),
                                              window);

  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (vnck_selector_activate_window),
                            window);

  if (!vnck_window_is_skip_tasklist (window))
    ctk_widget_show (item);

  g_object_set_data (G_OBJECT (item), "vnck-selector-window", window);

  return item;
}

static void
vnck_selector_insert_window (VnckSelector *selector, VnckWindow *window)
{
  CtkWidget     *item;
  VnckScreen    *screen;
  VnckWorkspace *workspace;
  int            workspace_n;
  int            i;

  screen = vnck_selector_get_screen (selector);
  workspace = vnck_window_get_workspace (window);

  if (!workspace && !vnck_window_is_pinned (window))
    return;

  item = vnck_selector_create_window (selector, window);

  if (!workspace || workspace == vnck_screen_get_active_workspace (screen))
    {
      /* window is pinned or in the current workspace
       * => insert before the separator */
      GList *l, *children;

      i = 0;

      children = ctk_container_get_children (CTK_CONTAINER (selector->priv->menu));
      for (l = children; l; l = l->next)
        {
          if (CTK_IS_SEPARATOR_MENU_ITEM (l->data))
            break;
          i++;
        }
      g_list_free (children);

      ctk_menu_shell_insert (CTK_MENU_SHELL (selector->priv->menu),
                             item, i);
    }
  else
    {
      workspace_n = vnck_workspace_get_number (workspace);

      if (workspace_n == vnck_screen_get_workspace_count (screen) - 1)
        /* window is in last workspace => just append */
        ctk_menu_shell_append (CTK_MENU_SHELL (selector->priv->menu), item);
      else
        {
          /* insert just before the next workspace item */
          GList *l, *children;

          i = 0;

          children = ctk_container_get_children (CTK_CONTAINER (selector->priv->menu));
          for (l = children; l; l = l->next)
            {
              int j;
              j = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                                                      "vnck-selector-workspace-n"));
              if (j - 1 == workspace_n + 1)
                break;
              i++;
            }
          g_list_free (children);

          ctk_menu_shell_insert (CTK_MENU_SHELL (selector->priv->menu),
                                 item, i);
        }
    }
}

static void
vnck_selector_append_window (VnckSelector *selector, VnckWindow *window)
{
  CtkWidget *item;

  item = vnck_selector_create_window (selector, window);
  ctk_menu_shell_append (CTK_MENU_SHELL (selector->priv->menu), item);
}

static void
vnck_selector_window_opened (VnckScreen   *screen G_GNUC_UNUSED,
			     VnckWindow   *window,
			     VnckSelector *selector)
{
  vnck_selector_connect_to_window (selector, window);

  if (!selector->priv->menu || !ctk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  vnck_selector_insert_window (selector, window);
  vnck_selector_make_menu_consistent (selector);

  ctk_menu_reposition (CTK_MENU (selector->priv->menu));
}

static void
vnck_selector_window_closed (VnckScreen   *screen G_GNUC_UNUSED,
			     VnckWindow   *window,
			     VnckSelector *selector)
{
  CtkWidget *item;

  if (window == selector->priv->icon_window)
    vnck_selector_set_active_window (selector, NULL);

  if (!selector->priv->menu || !ctk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (!item)
    return;

  g_object_set_data (G_OBJECT (item), "vnck-selector-window", NULL);

  ctk_widget_hide (item);
  vnck_selector_make_menu_consistent (selector);

  ctk_menu_reposition (CTK_MENU (selector->priv->menu));
}

static void
vnck_selector_workspace_created (VnckScreen    *screen,
                                 VnckWorkspace *workspace,
                                 VnckSelector  *selector)
{
  if (!selector->priv->menu || !ctk_widget_get_visible (selector->priv->menu))
    return;

  /* this is assuming that the new workspace will have a higher number
   * than all the old workspaces, which is okay since the old workspaces
   * didn't disappear in the meantime */
  vnck_selector_add_workspace (selector, screen,
                               vnck_workspace_get_number (workspace));

  vnck_selector_make_menu_consistent (selector);

  ctk_menu_reposition (CTK_MENU (selector->priv->menu));
}

static void
vnck_selector_workspace_destroyed (VnckScreen    *screen G_GNUC_UNUSED,
				   VnckWorkspace *workspace,
				   VnckSelector  *selector)
{
  GList     *l, *children;
  CtkWidget *destroy;
  int        i;

  if (!selector->priv->menu || !ctk_widget_get_visible (selector->priv->menu))
    return;

  destroy = NULL;

  i = vnck_workspace_get_number (workspace);

  /* search for the item of this workspace so that we destroy it */
  children = ctk_container_get_children (CTK_CONTAINER (selector->priv->menu));

  for (l = children; l; l = l->next)
    {
      int j;

      j = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                           "vnck-selector-workspace-n"));


      if (j - 1 == i)
        destroy = CTK_WIDGET (l->data);
      else if (j - 1 > i)
        /* shift the following workspaces */
        g_object_set_data (G_OBJECT (l->data), "vnck-selector-workspace-n",
                           GINT_TO_POINTER (j - 1));
    }

  g_list_free (children);

  if (destroy)
    ctk_widget_destroy (destroy);

  vnck_selector_make_menu_consistent (selector);

  ctk_menu_reposition (CTK_MENU (selector->priv->menu));
}

static void
vnck_selector_connect_to_window (VnckSelector *selector, VnckWindow *window)
{
  vncklet_connect_while_alive (window, "icon_changed",
                               G_CALLBACK (vnck_selector_window_icon_changed),
                               selector, selector);
  vncklet_connect_while_alive (window, "name_changed",
                               G_CALLBACK (vnck_selector_window_name_changed),
                               selector, selector);
  vncklet_connect_while_alive (window, "state_changed",
                               G_CALLBACK (vnck_selector_window_state_changed),
                               selector, selector);
  vncklet_connect_while_alive (window, "workspace_changed",
                               G_CALLBACK (vnck_selector_window_workspace_changed),
                               selector, selector);
}

static void
vnck_selector_disconnect_from_window (VnckSelector *selector,
                                      VnckWindow   *window)
{
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_selector_window_icon_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_selector_window_name_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_selector_window_state_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_selector_window_workspace_changed,
                                        selector);
}

static void
vnck_selector_connect_to_screen (VnckSelector *selector, VnckScreen *screen)
{
  vncklet_connect_while_alive (screen, "active_window_changed",
                               G_CALLBACK
                               (vnck_selector_active_window_changed),
                               selector, selector);

  vncklet_connect_while_alive (screen, "window_opened",
                               G_CALLBACK (vnck_selector_window_opened),
                               selector, selector);

  vncklet_connect_while_alive (screen, "window_closed",
                               G_CALLBACK (vnck_selector_window_closed),
                               selector, selector);

  vncklet_connect_while_alive (screen, "workspace_created",
                               G_CALLBACK (vnck_selector_workspace_created),
                               selector, selector);

  vncklet_connect_while_alive (screen, "workspace_destroyed",
                               G_CALLBACK (vnck_selector_workspace_destroyed),
                               selector, selector);
}

static void
vnck_selector_disconnect_from_screen (VnckSelector *selector,
                                      VnckScreen   *screen)
{
  g_signal_handlers_disconnect_by_func (screen,
                                        vnck_selector_active_window_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        vnck_selector_window_opened,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        vnck_selector_window_closed,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        vnck_selector_workspace_created,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        vnck_selector_workspace_destroyed,
                                        selector);
}

static void
vnck_selector_destroy_menu (CtkWidget    *widget G_GNUC_UNUSED,
			    VnckSelector *selector)
{
  selector->priv->menu = NULL;

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = NULL;

  selector->priv->no_windows_item = NULL;
}

static gboolean
vnck_selector_scroll_event (CtkWidget      *widget,
                            CdkEventScroll *event)
{
  VnckSelector *selector;
  VnckScreen *screen;
  VnckWorkspace *workspace;
  GList *windows_list;
  GList *l;
  VnckWindow *window;
  VnckWindow *previous_window;
  gboolean should_activate_next_window;

  selector = VNCK_SELECTOR (widget);

  screen = vnck_selector_get_screen (selector);
  workspace = vnck_screen_get_active_workspace (screen);
  windows_list = vnck_screen_get_windows (screen);
  windows_list = g_list_sort (windows_list, vnck_selector_windows_compare);

  /* Walk through the list of windows until we find the active one
   * (considering only those windows on the same workspace).
   * Then, depending on whether we're scrolling up or down, activate the next
   * window in the list (if it exists), or the previous one.
   */
  previous_window = NULL;
  should_activate_next_window = FALSE;
  for (l = windows_list; l; l = l->next)
    {
      window = VNCK_WINDOW (l->data);

      if (vnck_window_is_skip_tasklist (window))
        continue;

      if (workspace && !vnck_window_is_pinned (window) &&
          vnck_window_get_workspace (window) != workspace)
        continue;

      if (should_activate_next_window)
        {
          vnck_window_activate_transient (window, event->time);
          return TRUE;
        }

      if (vnck_window_is_active (window))
        {
          switch (event->direction)
            {
              case CDK_SCROLL_UP:
                if (previous_window != NULL)
                  {
                    vnck_window_activate_transient (previous_window,
                                                    event->time);
                    return TRUE;
                  }
              break;

              case CDK_SCROLL_DOWN:
                should_activate_next_window = TRUE;
              break;

              case CDK_SCROLL_LEFT:
              case CDK_SCROLL_RIGHT:
                /* We ignore LEFT and RIGHT scroll events. */
              break;

              case CDK_SCROLL_SMOOTH:
              break;

              default:
                g_assert_not_reached ();
            }
        }

      previous_window = window;
    }

  return TRUE;
}

static void
vnck_selector_menu_hidden (CtkWidget    *menu G_GNUC_UNUSED,
			   VnckSelector *selector)
{
  ctk_widget_set_state_flags (CTK_WIDGET (selector), CTK_STATE_FLAG_NORMAL, TRUE);
}

static void
vnck_selector_on_show (CtkWidget    *widget G_GNUC_UNUSED,
		       VnckSelector *selector)
{
  CtkWidget *separator;
  VnckScreen *screen;
  VnckWorkspace *workspace;
  int nb_workspace;
  int i;
  GList **windows_per_workspace;
  GList *windows;
  GList *l, *children;

  /* Remove existing items */
  children = ctk_container_get_children (CTK_CONTAINER (selector->priv->menu));
  for (l = children; l; l = l->next)
    ctk_container_remove (CTK_CONTAINER (selector->priv->menu), l->data);
  g_list_free (children);

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 NULL, NULL);

  screen = vnck_selector_get_screen (selector);

  nb_workspace = vnck_screen_get_workspace_count (screen);
  windows_per_workspace = g_malloc0 (nb_workspace * sizeof (GList *));

  /* Get windows ordered by workspaces */
  windows = vnck_screen_get_windows (screen);
  windows = g_list_sort (windows, vnck_selector_windows_compare);

  for (l = windows; l; l = l->next)
    {
      workspace = vnck_window_get_workspace (l->data);
      if (!workspace && vnck_window_is_pinned (l->data))
        workspace = vnck_screen_get_active_workspace (screen);
      if (!workspace)
        continue;
      i = vnck_workspace_get_number (workspace);
      windows_per_workspace[i] = g_list_prepend (windows_per_workspace[i],
                                                 l->data);
    }

  /* Add windows from the current workspace */
  workspace = vnck_screen_get_active_workspace (screen);
  if (workspace)
    {
      i = vnck_workspace_get_number (workspace);

      windows_per_workspace[i] = g_list_reverse (windows_per_workspace[i]);
      for (l = windows_per_workspace[i]; l; l = l->next)
        vnck_selector_append_window (selector, l->data);
      g_list_free (windows_per_workspace[i]);
      windows_per_workspace[i] = NULL;
    }

  /* Add separator */
  separator = ctk_separator_menu_item_new ();
  ctk_menu_shell_append (CTK_MENU_SHELL (selector->priv->menu), separator);

  /* Add windows from other workspaces */
  for (i = 0; i < nb_workspace; i++)
    {
      vnck_selector_add_workspace (selector, screen, i);
      windows_per_workspace[i] = g_list_reverse (windows_per_workspace[i]);
      for (l = windows_per_workspace[i]; l; l = l->next)
        vnck_selector_append_window (selector, l->data);
      g_list_free (windows_per_workspace[i]);
      windows_per_workspace[i] = NULL;
    }
  g_free (windows_per_workspace);

  selector->priv->no_windows_item = vnck_selector_item_new (selector,
		  					    _("No Windows Open"),
							    NULL);
  ctk_widget_set_sensitive (selector->priv->no_windows_item, FALSE);
  ctk_menu_shell_append (CTK_MENU_SHELL (selector->priv->menu),
                         selector->priv->no_windows_item);

  vnck_selector_make_menu_consistent (selector);
}

static void
vnck_selector_fill (VnckSelector *selector)
{
  CtkWidget      *menu_item;
  CtkCssProvider *provider;

  menu_item = ctk_menu_item_new ();
  ctk_widget_show (menu_item);
  ctk_menu_shell_append (CTK_MENU_SHELL (selector), menu_item);

  selector->priv->image = ctk_image_new ();
  ctk_widget_show (selector->priv->image);
  ctk_container_add (CTK_CONTAINER (menu_item), selector->priv->image);

  selector->priv->menu = ctk_menu_new ();
  ctk_menu_item_set_submenu (CTK_MENU_ITEM (menu_item),
                             selector->priv->menu);
  g_signal_connect (selector->priv->menu, "hide",
                    G_CALLBACK (vnck_selector_menu_hidden), selector);
  g_signal_connect (selector->priv->menu, "destroy",
                    G_CALLBACK (vnck_selector_destroy_menu), selector);
  g_signal_connect (selector->priv->menu, "show",
                    G_CALLBACK (vnck_selector_on_show), selector);

  ctk_widget_set_name (CTK_WIDGET (selector),
                       "gnome-panel-window-menu-menu-bar");

  provider = ctk_css_provider_new ();
  ctk_css_provider_load_from_data (provider,
                                   "#gnome-panel-window-menu-menu-bar {\n"
                                   " border-width: 0px;\n"
                                   "}",
                                   -1, NULL);
  ctk_style_context_add_provider (ctk_widget_get_style_context (CTK_WIDGET (selector)),
                                  CTK_STYLE_PROVIDER (provider),
                                  CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  ctk_widget_show (CTK_WIDGET (selector));
}

static void
vnck_selector_init (VnckSelector *selector)
{
  AtkObject *atk_obj;

  atk_obj = ctk_widget_get_accessible (CTK_WIDGET (selector));
  atk_object_set_name (atk_obj, _("Window Selector"));
  atk_object_set_description (atk_obj, _("Tool to switch between windows"));

  selector->priv = vnck_selector_get_instance_private (selector);

  ctk_widget_add_events (CTK_WIDGET (selector), CDK_SCROLL_MASK);
}

static void
vnck_selector_class_init (VnckSelectorClass *klass)
{
  GObjectClass   *object_class     = G_OBJECT_CLASS (klass);
  CtkWidgetClass *widget_class     = CTK_WIDGET_CLASS (klass);

  object_class->constructor = vnck_selector_constructor;
  object_class->dispose = vnck_selector_dispose;
  object_class->finalize = vnck_selector_finalize;

  widget_class->realize   = vnck_selector_realize;
  widget_class->unrealize = vnck_selector_unrealize;
  widget_class->scroll_event = vnck_selector_scroll_event;

  ctk_widget_class_set_css_name (widget_class, "vnck-selector");
}

static GObject *
vnck_selector_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
  GObject *obj;

  obj = G_OBJECT_CLASS (vnck_selector_parent_class)->constructor (
                                                      type,
                                                      n_construct_properties,
                                                      construct_properties);

  vnck_selector_fill (VNCK_SELECTOR (obj));

  return obj;
}

static void
vnck_selector_finalize (GObject *object)
{
  VnckSelector *selector;

  selector = VNCK_SELECTOR (object);

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = NULL;

  G_OBJECT_CLASS (vnck_selector_parent_class)->finalize (object);
}

static void
vnck_selector_dispose (GObject *object)
{
  VnckSelector *selector;

  selector = VNCK_SELECTOR (object);

  if (selector->priv->menu)
    ctk_widget_destroy (selector->priv->menu);
  selector->priv->menu = NULL;

  selector->priv->image       = NULL;
  selector->priv->icon_window = NULL;

  G_OBJECT_CLASS (vnck_selector_parent_class)->dispose (object);
}

static void
vnck_selector_realize (CtkWidget *widget)
{
  VnckSelector *selector;
  VnckScreen   *screen;
  VnckWindow   *window;
  GList        *l;

  CTK_WIDGET_CLASS (vnck_selector_parent_class)->realize (widget);

  selector = VNCK_SELECTOR (widget);
  screen = vnck_selector_get_screen (selector);

  window = vnck_screen_get_active_window (screen);
  vnck_selector_set_active_window (selector, window);

  for (l = vnck_screen_get_windows (screen); l; l = l->next)
    vnck_selector_connect_to_window (selector, l->data);

  vnck_selector_connect_to_screen (selector, screen);
}

static void
vnck_selector_unrealize (CtkWidget *widget)
{
  VnckSelector *selector;
  VnckScreen   *screen;
  GList        *l;

  selector = VNCK_SELECTOR (widget);
  screen = vnck_selector_get_screen (selector);

  vnck_selector_disconnect_from_screen (selector, screen);

  for (l = vnck_screen_get_windows (screen); l; l = l->next)
    vnck_selector_disconnect_from_window (selector, l->data);

  CTK_WIDGET_CLASS (vnck_selector_parent_class)->unrealize (widget);
}

/**
 * vnck_selector_new:
 *
 * Creates a new #VnckSelector. The #VnckSelector will list #VnckWindow of the
 * #VnckScreen it is on.
 *
 * Return value: a newly created #VnckSelector.
 *
 * Since: 2.10
 */
CtkWidget *
vnck_selector_new (void)
{
  VnckSelector *selector;

  selector = g_object_new (VNCK_TYPE_SELECTOR, NULL);

  return CTK_WIDGET (selector);
}
