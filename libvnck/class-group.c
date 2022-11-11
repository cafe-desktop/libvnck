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

#undef VNCK_DISABLE_DEPRECATED

#include <string.h>
#include "class-group.h"
#include "window.h"
#include "private.h"

/**
 * SECTION:class-group
 * @short_description: an object representing a group of windows of the same
 * class.
 * @see_also: vnck_window_get_class_group()
 * @stability: Unstable
 *
 * The #VnckClassGroup is a group of #VnckWindow that are all in the same
 * class. It can be used to represent windows by classes, group windows by
 * classes or to manipulate all windows of a particular class.
 *
 * The class of a window is defined by the WM_CLASS property of this window.
 * More information about the WM_CLASS property is available in the <ulink
 * url="http://tronche.com/gui/x/icccm/sec-4.html&num;s-4.1.2.5">WM_CLASS Property</ulink>
 * section (section 4.1.2.5) of the <ulink
 * url="http://tronche.com/gui/x/icccm/">ICCCM</ulink>.
 *
 * The #VnckClassGroup objects are always owned by libvnck and must not be
 * referenced or unreferenced.
 */

/* Private part of the VnckClassGroup structure */
struct _VnckClassGroupPrivate {
  VnckScreen *screen;

  char *res_class;
  char *name;
  GList *windows;
  GHashTable *window_icon_handlers;
  GHashTable *window_name_handlers;

  CdkPixbuf *icon;
  CdkPixbuf *mini_icon;
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckClassGroup, vnck_class_group, G_TYPE_OBJECT);

/* Hash table that maps res_class strings -> VnckClassGroup instances */
static GHashTable *class_group_hash = NULL;

static void vnck_class_group_finalize    (GObject             *object);

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

void
_vnck_class_group_shutdown_all (void)
{
  if (class_group_hash != NULL)
    {
      g_hash_table_destroy (class_group_hash);
      class_group_hash = NULL;
    }
}

static void
vnck_class_group_class_init (VnckClassGroupClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = vnck_class_group_finalize;

  /**
   * VnckClassGroup::name-changed:
   * @class_group: the #VnckClassGroup which emitted the signal.
   *
   * Emitted when the name of @class_group changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckClassGroupClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * VnckClassGroup::icon-changed:
   * @class_group: the #VnckClassGroup which emitted the signal.
   *
   * Emitted when the icon of @class_group changes.
   */
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckClassGroupClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
vnck_class_group_init (VnckClassGroup *class_group)
{
  class_group->priv = vnck_class_group_get_instance_private (class_group);
  class_group->priv->window_icon_handlers = g_hash_table_new (g_direct_hash,
                                                              g_direct_equal);
  class_group->priv->window_name_handlers = g_hash_table_new (g_direct_hash,
                                                              g_direct_equal);
}

static void
remove_signal_handler (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  g_signal_handler_disconnect (key, (gulong) value);
}

static void
vnck_class_group_finalize (GObject *object)
{
  VnckClassGroup *class_group;

  class_group = VNCK_CLASS_GROUP (object);

  if (class_group->priv->res_class)
    {
      g_free (class_group->priv->res_class);
      class_group->priv->res_class = NULL;
    }

  if (class_group->priv->name)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = NULL;
    }

  if (class_group->priv->windows)
    {
      g_list_free (class_group->priv->windows);
      class_group->priv->windows = NULL;
    }

  if (class_group->priv->window_icon_handlers)
    {
      g_hash_table_foreach (class_group->priv->window_icon_handlers,
                            remove_signal_handler,
                            NULL);
      g_hash_table_destroy (class_group->priv->window_icon_handlers);
      class_group->priv->window_icon_handlers = NULL;
    }

  if (class_group->priv->window_name_handlers)
    {
      g_hash_table_foreach (class_group->priv->window_name_handlers,
                            remove_signal_handler,
                            NULL);
      g_hash_table_destroy (class_group->priv->window_name_handlers);
      class_group->priv->window_name_handlers = NULL;
    }

  if (class_group->priv->icon)
    {
      g_object_unref (class_group->priv->icon);
      class_group->priv->icon = NULL;
    }

  if (class_group->priv->mini_icon)
    {
      g_object_unref (class_group->priv->mini_icon);
      class_group->priv->mini_icon = NULL;
    }

  G_OBJECT_CLASS (vnck_class_group_parent_class)->finalize (object);
}

/**
 * vnck_class_group_get:
 * @id: identifier name of the sought resource class.
 *
 * Gets the #VnckClassGroup corresponding to @id.
 *
 * Return value: (transfer none): the #VnckClassGroup corresponding to
 * @id, or %NULL if there is no #VnckClassGroup with the specified
 * @id. The returned #VnckClassGroup is owned by libvnck and must not be
 * referenced or unreferenced.
 *
 * Since: 2.2
 **/
VnckClassGroup *
vnck_class_group_get (const char *id)
{
  if (!class_group_hash)
    return NULL;
  else
    return g_hash_table_lookup (class_group_hash, id ? id : "");
}

/**
 * _vnck_class_group_create:
 * @screen: a #VnckScreen.
 * @res_class: name of the resource class for the group.
 *
 * Creates a new VnckClassGroup with the specified resource class name.  If
 * @res_class is #NULL, then windows without a resource class name will get
 * grouped under this class group.
 *
 * Return value: a newly-created #VnckClassGroup, or an existing one that
 * matches the @res_class.
 **/
VnckClassGroup *
_vnck_class_group_create (VnckScreen *screen,
                          const char *res_class)
{
  VnckClassGroup *class_group;

  if (class_group_hash == NULL)
    class_group_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, g_object_unref);

  g_return_val_if_fail (g_hash_table_lookup (class_group_hash, res_class ? res_class : "") == NULL,
			NULL);

  class_group = g_object_new (VNCK_TYPE_CLASS_GROUP, NULL);
  class_group->priv->screen = screen;

  class_group->priv->res_class = g_strdup (res_class ? res_class : "");

  g_hash_table_insert (class_group_hash,
                       class_group->priv->res_class, class_group);
  /* Hash now owns one ref, caller gets none */

  return class_group;
}

/**
 * _vnck_class_group_destroy:
 * @class_group: a #VnckClassGroup.
 *
 * Destroys the specified @class_group.
 **/
void
_vnck_class_group_destroy (VnckClassGroup *class_group)
{
  g_return_if_fail (VNCK_IS_CLASS_GROUP (class_group));

  g_hash_table_remove (class_group_hash, class_group->priv->res_class);

  /* Removing from hash also removes the only ref VnckClassGroup had */
}

static const char *
get_name_from_applications (VnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from the group leaders.  If all have the same name, we
   * can use that.
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      VnckWindow *w;
      VnckApplication *app;

      w = VNCK_WINDOW (l->data);
      app = vnck_window_get_application (w);

      if (!first_name)
	{
	  if (app)
	    first_name = vnck_application_get_name (app);
	}
      else
	{
	  if (!app || strcmp (first_name, vnck_application_get_name (app)) != 0)
	    break;
	}
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}

static const char *
get_name_from_windows (VnckClassGroup *class_group)
{
  const char *first_name;
  GList *l;

  /* Try to get the name from windows, following the same rationale as
   * get_name_from_applications()
   */

  first_name = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      VnckWindow *window;

      window = VNCK_WINDOW (l->data);

      if (!first_name)
	first_name = vnck_window_get_name (window);
      else
	if (strcmp (first_name, vnck_window_get_name (window)) != 0)
	  break;
    }

  if (!l)
    {
      /* All names are the same, so use one of them */
      return first_name;
    }
  else
    return NULL;
}


/* Gets a sensible name for the class group from the application group leaders
 * or from individual windows.
 */
static void
set_name (VnckClassGroup *class_group)
{
  const char *new_name;

  new_name = get_name_from_applications (class_group);

  if (!new_name)
    {
      new_name = get_name_from_windows (class_group);

      if (!new_name)
	new_name = class_group->priv->res_class;
    }

  g_assert (new_name != NULL);

  if (!class_group->priv->name ||
      strcmp (class_group->priv->name, new_name) != 0)
    {
      g_free (class_group->priv->name);
      class_group->priv->name = g_strdup (new_name);

      g_signal_emit (G_OBJECT (class_group), signals[NAME_CHANGED], 0);
    }
}

/* Walks the list of applications, trying to get an icon from them */
static void
get_icons_from_applications (VnckClassGroup *class_group, CdkPixbuf **icon, CdkPixbuf **mini_icon)
{
  GList *l;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      VnckWindow *window;
      VnckApplication *app;

      window = VNCK_WINDOW (l->data);
      app = vnck_window_get_application (window);
      if (app)
	{
	  *icon = vnck_application_get_icon (app);
	  *mini_icon = vnck_application_get_mini_icon (app);

	  if (*icon && *mini_icon)
	    return;
	  else
	    {
	      *icon = NULL;
	      *mini_icon = NULL;
	    }
	}
    }
}

/* Walks the list of windows, trying to get an icon from them */
static void
get_icons_from_windows (VnckClassGroup *class_group, CdkPixbuf **icon, CdkPixbuf **mini_icon)
{
  GList *l;

  *icon = NULL;
  *mini_icon = NULL;

  for (l = class_group->priv->windows; l; l = l->next)
    {
      VnckWindow *window;

      window = VNCK_WINDOW (l->data);

      *icon = vnck_window_get_icon (window);
      *mini_icon = vnck_window_get_mini_icon (window);

      if (*icon && *mini_icon)
	return;
      else
	{
	  *icon = NULL;
	  *mini_icon = NULL;
	}
    }
}

/* Gets a sensible icon and mini_icon for the class group from the application
 * group leaders or from individual windows.
 */
static void
set_icon (VnckClassGroup *class_group)
{
  CdkPixbuf *icon, *mini_icon;
  gboolean icons_reffed = FALSE;

  get_icons_from_applications (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
    get_icons_from_windows (class_group, &icon, &mini_icon);

  if (!icon || !mini_icon)
    {
      _vnck_get_fallback_icons (&icon,
                                _vnck_get_default_icon_size (),
                                _vnck_get_default_icon_size (),
                                &mini_icon,
                                _vnck_get_default_mini_icon_size (),
                                _vnck_get_default_mini_icon_size ());
      icons_reffed = TRUE;
    }

  g_assert (icon && mini_icon);

  if (class_group->priv->icon)
    g_object_unref (class_group->priv->icon);

  if (class_group->priv->mini_icon)
    g_object_unref (class_group->priv->mini_icon);

  class_group->priv->icon = icon;
  class_group->priv->mini_icon = mini_icon;

  if (!icons_reffed)
    {
      g_object_ref (class_group->priv->icon);
      g_object_ref (class_group->priv->mini_icon);
    }

  g_signal_emit (G_OBJECT (class_group), signals[ICON_CHANGED], 0);
}


/* Handle window's icon_changed signal, update class group icon */
static void
update_class_group_icon (VnckWindow     *window,
                         VnckClassGroup *class_group)
{
  set_icon (class_group);
}

/* Handle window's name_changed signal, update class group name */
static void
update_class_group_name (VnckWindow     *window,
                         VnckClassGroup *class_group)
{
  set_name (class_group);
}

static void
window_weak_notify_cb (gpointer  data,
                       GObject  *where_the_window_was)
{
  VnckClassGroup *class_group;
  VnckClassGroupPrivate *priv;

  class_group = VNCK_CLASS_GROUP (data);
  priv = class_group->priv;

  g_hash_table_remove (priv->window_icon_handlers, where_the_window_was);
  g_hash_table_remove (priv->window_name_handlers, where_the_window_was);
}

/**
 * _vnck_class_group_add_window:
 * @class_group: a #VnckClassGroup.
 * @window: a #VnckWindow.
 *
 * Adds a window to @class_group.  You should only do this if the resource
 * class of the window matches the @class_group<!-- -->'s.
 **/
void
_vnck_class_group_add_window (VnckClassGroup *class_group,
                              VnckWindow     *window)
{
  gulong signal_id;

  g_return_if_fail (VNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (vnck_window_get_class_group (window) == NULL);

  class_group->priv->windows = g_list_prepend (class_group->priv->windows,
                                               window);
  _vnck_window_set_class_group (window, class_group);

  signal_id = g_signal_connect (window,
                                "icon-changed",
                                G_CALLBACK (update_class_group_icon),
                                class_group);
  g_hash_table_insert (class_group->priv->window_icon_handlers,
                       window,
                       (gpointer) signal_id);

  signal_id = g_signal_connect (window,
                                "name-changed",
                                G_CALLBACK (update_class_group_name),
                                class_group);
  g_hash_table_insert (class_group->priv->window_name_handlers,
                       window,
                       (gpointer) signal_id);

  g_object_weak_ref (G_OBJECT (window), window_weak_notify_cb, class_group);

  set_name (class_group);
  set_icon (class_group);

  /* FIXME: should we monitor class group changes on the window?  The ICCCM says
   * that clients should never change WM_CLASS unless the window is withdrawn.
   */
}

/**
 * _vnck_class_group_remove_window:
 * @class_group: a #VnckClassGroup.
 * @window: a #VnckWindow.
 *
 * Removes a window from the list of windows that are grouped under the
 * specified @class_group.
 **/
void
_vnck_class_group_remove_window (VnckClassGroup *class_group,
				 VnckWindow     *window)
{
  gulong icon_handler, name_handler;

  g_return_if_fail (VNCK_IS_CLASS_GROUP (class_group));
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (vnck_window_get_class_group (window) == class_group);

  class_group->priv->windows = g_list_remove (class_group->priv->windows,
                                              window);
  _vnck_window_set_class_group (window, NULL);
  icon_handler = (gulong) g_hash_table_lookup (class_group->priv->window_icon_handlers,
                                               window);
  if (icon_handler != 0)
    {
      g_signal_handler_disconnect (window,
                                   icon_handler);
      g_hash_table_remove (class_group->priv->window_icon_handlers,
                           window);
    }
  name_handler = (gulong) g_hash_table_lookup (class_group->priv->window_name_handlers,
                                               window);
  if (name_handler != 0)
    {
      g_signal_handler_disconnect (window,
                                   name_handler);
      g_hash_table_remove (class_group->priv->window_name_handlers,
                           window);
    }

  set_name (class_group);
  set_icon (class_group);
}

/**
 * vnck_class_group_get_windows:
 * @class_group: a #VnckClassGroup.
 *
 * Gets the list of #VnckWindow that are grouped in @class_group.
 *
 * Return value: (element-type VnckWindow) (transfer none): the list of
 * #VnckWindow grouped in @class_group, or %NULL if the group contains no
 * window. The list should not be modified nor freed, as it is owned by
 * @class_group.
 *
 * Since: 2.2
 **/
GList *
vnck_class_group_get_windows (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->windows;
}

/**
 * vnck_class_group_get_id:
 * @class_group: a #VnckClassGroup.
 *
 * Gets the identifier name for @class_group. This is the resource class for
 * @class_group.
 *
 * Return value: the identifier name of @class_group, or an
 * empty string if the group has no identifier name.
 *
 * Since: 3.2
 **/
const char *
vnck_class_group_get_id (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->res_class;
}

/**
 * vnck_class_group_get_res_class:
 * @class_group: a #VnckClassGroup.
 *
 * Gets the resource class name for @class_group.
 *
 * Return value: the resource class name of @class_group, or an
 * empty string if the group has no resource class name.
 *
 * Since: 2.2
 * Deprecated:3.2: Use vnck_class_group_get_id() instead.
 **/
const char *
vnck_class_group_get_res_class (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->res_class;
}

/**
 * vnck_class_group_get_name:
 * @class_group: a #VnckClassGroup.
 *
 * Gets an human-readable name for @class_group. Since there is no way to
 * properly find this name, a suboptimal heuristic is used to find it. The name
 * is the name of all #VnckApplication for each #VnckWindow in @class_group if
 * they all have the same name. If all #VnckApplication don't have the same
 * name, the name is the name of all #VnckWindow in @class_group if they all
 * have the same name. If all #VnckWindow don't have the same name, the
 * resource class name is used.
 *
 * Return value: an human-readable name for @class_group.
 *
 * Since: 2.2
 **/
const char *
vnck_class_group_get_name (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->name;
}

/**
 * vnck_class_group_get_icon:
 * @class_group: a #VnckClassGroup.
 *
 * Gets the icon to be used for @class_group. Since there is no way to
 * properly find the icon, a suboptimal heuristic is used to find it. The icon
 * is the first icon found by looking at all the #VnckApplication for each
 * #VnckWindow in @class_group, then at all the #VnckWindow in @class_group. If
 * no icon was found, a fallback icon is used.
 *
 * Return value: (transfer none): the icon for @class_group. The caller should
 * reference the returned <classname>CdkPixbuf</classname> if it needs to keep
 * the icon around.
 *
 * Since: 2.2
 **/
CdkPixbuf *
vnck_class_group_get_icon (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->icon;
}

/**
 * vnck_class_group_get_mini_icon:
 * @class_group: a #VnckClassGroup.
 *
 * Gets the mini-icon to be used for @class_group. Since there is no way to
 * properly find the mini-icon, the same suboptimal heuristic as the one for
 * vnck_class_group_get_icon() is used to find it.
 *
 * Return value: (transfer none): the mini-icon for @class_group. The caller
 * should reference the returned <classname>CdkPixbuf</classname> if it needs
 * to keep the mini-icon around.
 *
 * Since: 2.2
 **/
CdkPixbuf *
vnck_class_group_get_mini_icon (VnckClassGroup *class_group)
{
  g_return_val_if_fail (class_group != NULL, NULL);

  return class_group->priv->mini_icon;
}
