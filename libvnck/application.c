/* application object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
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
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include "application.h"
#include "private.h"

/**
 * SECTION:application
 * @short_description: an object representing a group of windows of the same
 * application.
 * @see_also: vnck_window_get_application()
 * @stability: Unstable
 *
 * The #VnckApplication is a group of #VnckWindow that are all in the same
 * application. It can be used to represent windows by applications, group
 * windows by applications or to manipulate all windows of a particular
 * application.
 *
 * A #VnckApplication is identified by the group leader of the #VnckWindow
 * belonging to it, and new #VnckWindow are added to a #VnckApplication if and
 * only if they have the group leader of the #VnckApplication.
 *
 * The #VnckApplication objects are always owned by libvnck and must not be
 * referenced or unreferenced.
 */

#define FALLBACK_NAME _("Untitled application")

static GHashTable *app_hash = NULL;

struct _VnckApplicationPrivate
{
  Window xwindow; /* group leader */
  VnckScreen *screen;
  GList *windows;
  int pid;
  char *name;

  int orig_event_mask;

  VnckWindow *name_window;    /* window we are using name of */

  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  VnckIconCache *icon_cache;

  char *startup_id;

  guint name_from_leader : 1; /* name is from group leader */
  guint icon_from_leader : 1;

  guint need_emit_icon_changed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckApplication, vnck_application, G_TYPE_OBJECT);

enum {
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

static void emit_name_changed (VnckApplication *app);
static void emit_icon_changed (VnckApplication *app);

static void reset_name  (VnckApplication *app);
static void update_name (VnckApplication *app);

static void vnck_application_finalize    (GObject        *object);

static guint signals[LAST_SIGNAL] = { 0 };

void
_vnck_application_shutdown_all (void)
{
  if (app_hash != NULL)
    {
      g_hash_table_destroy (app_hash);
      app_hash = NULL;
    }
}

static void
vnck_application_init (VnckApplication *application)
{
  application->priv = vnck_application_get_instance_private (application);

  application->priv->icon_cache = _vnck_icon_cache_new ();
  _vnck_icon_cache_set_want_fallback (application->priv->icon_cache, FALSE);
}

static void
vnck_application_class_init (VnckApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vnck_application_finalize;

  /**
   * VnckApplication::name-changed:
   * @app: the #VnckApplication which emitted the signal.
   *
   * Emitted when the name of @app changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckApplicationClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckApplication::icon-changed:
   * @app: the #VnckApplication which emitted the signal.
   *
   * Emitted when the icon of @app changes.
   */
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckApplicationClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
vnck_application_finalize (GObject *object)
{
  VnckApplication *application;

  application = VNCK_APPLICATION (object);

  _vnck_select_input (VNCK_SCREEN_XSCREEN (application->priv->screen),
                      application->priv->xwindow,
                      application->priv->orig_event_mask,
                      FALSE);

  application->priv->xwindow = None;

  g_list_free (application->priv->windows);
  application->priv->windows = NULL;

  g_free (application->priv->name);
  application->priv->name = NULL;

  if (application->priv->icon)
    g_object_unref (G_OBJECT (application->priv->icon));
  application->priv->icon = NULL;

  if (application->priv->mini_icon)
    g_object_unref (G_OBJECT (application->priv->mini_icon));
  application->priv->mini_icon = NULL;

  _vnck_icon_cache_free (application->priv->icon_cache);
  application->priv->icon_cache = NULL;

  g_free (application->priv->startup_id);
  application->priv->startup_id = NULL;

  G_OBJECT_CLASS (vnck_application_parent_class)->finalize (object);
}

/**
 * vnck_application_get:
 * @xwindow: the X window ID of a group leader.
 *
 * Gets the #VnckApplication corresponding to the group leader with @xwindow
 * as X window ID.
 *
 * Return value: (transfer none): the #VnckApplication corresponding to
 * @xwindow, or %NULL if there no such #VnckApplication could be found. The
 * returned #VnckApplication is owned by libvnck and must not be referenced or
 * unreferenced.
 */
VnckApplication*
vnck_application_get (gulong xwindow)
{
  if (app_hash == NULL)
    return NULL;
  else
    return g_hash_table_lookup (app_hash, &xwindow);
}

/**
 * vnck_application_get_xid:
 * @app: a #VnckApplication.
 *
 * Gets the X window ID of the group leader window for @app.
 *
 * Return value: the X window ID of the group leader window for @app.
 **/
gulong
vnck_application_get_xid (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), 0);

  return app->priv->xwindow;
}

/**
 * vnck_application_get_windows:
 * @app: a #VnckApplication.
 *
 * Gets the list of #VnckWindow belonging to @app.
 *
 * Return value: (element-type VnckWindow) (transfer none): the list of
 * #VnckWindow belonging to @app, or %NULL if the application contains no
 * window. The list should not be modified nor freed, as it is owned by @app.
 **/
GList*
vnck_application_get_windows (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  return app->priv->windows;
}

/**
 * vnck_application_get_n_windows:
 * @app: a #VnckApplication.
 *
 * Gets the number of #VnckWindow belonging to @app.
 *
 * Return value: the number of #VnckWindow belonging to @app.
 **/
int
vnck_application_get_n_windows (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), 0);

  return g_list_length (app->priv->windows);
}

/**
 * vnck_application_get_name:
 * @app: a #VnckApplication.
 *
 * Gets the name of @app. Since there is no way to properly find this name,
 * various suboptimal heuristics are used to find it. CTK+ should probably have
 * a function to allow applications to set the _NET_WM_NAME property on the
 * group leader as the application name, and the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>
 * should say that this is where the application name goes.
 *
 * Return value: the name of @app, or a fallback name if no name is available.
 **/
const char*
vnck_application_get_name (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  if (app->priv->name)
    return app->priv->name;
  else
    return FALLBACK_NAME;
}

/**
 * vnck_application_get_icon_name:
 * @app: a #VnckApplication
 *
 * Gets the icon name of @app (to be used when @app is minimized). Since
 * there is no way to properly find this name, various suboptimal heuristics
 * are used to find it.
 *
 * Return value: the icon name of @app, or a fallback icon name if no icon name
 * is available.
 **/
const char*
vnck_application_get_icon_name (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  /* FIXME this isn't actually implemented, should be different
   * from regular name
   */

  if (app->priv->name)
    return app->priv->name;
  else
    return FALLBACK_NAME;
}

/**
 * vnck_application_get_pid:
 * @app: a #VnckApplication.
 *
 * Gets the process ID of @app.
 *
 * Return value: the process ID of @app, or 0 if none is available.
 **/
int
vnck_application_get_pid (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), 0);

  return app->priv->pid;
}

static void
get_icons (VnckApplication *app)
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  gsize normal_size;
  gsize mini_size;

  icon = NULL;
  mini_icon = NULL;
  normal_size = _vnck_get_default_icon_size ();
  mini_size = _vnck_get_default_mini_icon_size ();

  if (_vnck_read_icons (app->priv->screen,
                        app->priv->xwindow,
                        app->priv->icon_cache,
                        &icon, normal_size, normal_size,
                        &mini_icon, mini_size, mini_size))
    {
      app->priv->need_emit_icon_changed = TRUE;
      app->priv->icon_from_leader = TRUE;

      if (app->priv->icon)
        g_object_unref (G_OBJECT (app->priv->icon));

      if (app->priv->mini_icon)
        g_object_unref (G_OBJECT (app->priv->mini_icon));

      app->priv->icon = icon;
      app->priv->mini_icon = mini_icon;
    }

  /* FIXME we should really fall back to using the icon
   * for one of the windows. But then we need to be more
   * complicated about icon_changed and when the icon
   * needs updating and all that.
   */

  g_assert ((app->priv->icon && app->priv->mini_icon) ||
            !(app->priv->icon || app->priv->mini_icon));
}

void
_vnck_application_load_icons (VnckApplication *app)
{
  g_return_if_fail (VNCK_IS_APPLICATION (app));

  get_icons (app);
  if (app->priv->need_emit_icon_changed)
    emit_icon_changed (app);
}

/* Prefer to get group icon from a window of type "normal" */
static VnckWindow*
find_icon_window (VnckApplication *app)
{
  GList *tmp;

  tmp = app->priv->windows;
  while (tmp != NULL)
    {
      VnckWindow *w = tmp->data;

      if (vnck_window_get_window_type (w) == VNCK_WINDOW_NORMAL)
        return w;

      tmp = tmp->next;
    }

  if (app->priv->windows)
    return app->priv->windows->data;
  else
    return NULL;
}

/**
 * vnck_application_get_icon:
 * @app: a #VnckApplication.
 *
 * Gets the icon to be used for @app. If no icon is set for @app, a
 * suboptimal heuristic is used to find an appropriate icon. If no icon was
 * found, a fallback icon is used.
 *
 * Return value: (transfer none): the icon for @app. The caller should
 * reference the returned <classname>GdkPixbuf</classname> if it needs to keep
 * the icon around.
 **/
GdkPixbuf*
vnck_application_get_icon (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  _vnck_application_load_icons (app);

  if (app->priv->icon)
    return app->priv->icon;
  else
    {
      VnckWindow *w = find_icon_window (app);
      if (w)
        return vnck_window_get_icon (w);
      else
        return NULL;
    }
}

/**
 * vnck_application_get_mini_icon:
 * @app: a #VnckApplication.
 *
 * Gets the mini-icon to be used for @app. If no mini-icon is set for @app,
 * a suboptimal heuristic is used to find an appropriate icon. If no mini-icon
 * was found, a fallback mini-icon is used.
 *
 * Return value: (transfer none): the mini-icon for @app. The caller should
 * reference the returned <classname>GdkPixbuf</classname> if it needs to keep
 * the mini-icon around.
 **/
GdkPixbuf*
vnck_application_get_mini_icon (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  _vnck_application_load_icons (app);

  if (app->priv->mini_icon)
    return app->priv->mini_icon;
  else
    {
      VnckWindow *w = find_icon_window (app);
      if (w)
        return vnck_window_get_mini_icon (w);
      else
        return NULL;
    }
}

/**
 * vnck_application_get_icon_is_fallback:
 * @app: a #VnckApplication
 *
 * Gets whether a default fallback icon is used for @app (because none
 * was set on @app).
 *
 * Return value: %TRUE if the icon for @app is a fallback, %FALSE otherwise.
 **/
gboolean
vnck_application_get_icon_is_fallback (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), FALSE);

  if (app->priv->icon)
    return FALSE;
  else
    {
      VnckWindow *w = find_icon_window (app);
      if (w)
        return vnck_window_get_icon_is_fallback (w);
      else
        return TRUE;
    }
}

/**
 * vnck_application_get_startup_id:
 * @app: a #VnckApplication.
 *
 * Gets the startup sequence ID used for startup notification of @app.
 *
 * Return value: the startup sequence ID used for startup notification of @app,
 * or %NULL if none is available.
 *
 * Since: 2.2
 */
const char*
vnck_application_get_startup_id (VnckApplication *app)
{
  g_return_val_if_fail (VNCK_IS_APPLICATION (app), NULL);

  return app->priv->startup_id;
}

/* xwindow is a group leader */
VnckApplication*
_vnck_application_create (Window      xwindow,
                          VnckScreen *screen)
{
  VnckApplication *application;
  Screen          *xscreen;

  if (app_hash == NULL)
    app_hash = g_hash_table_new_full (_vnck_xid_hash, _vnck_xid_equal,
                                      NULL, g_object_unref);

  g_return_val_if_fail (g_hash_table_lookup (app_hash, &xwindow) == NULL,
                        NULL);

  xscreen = VNCK_SCREEN_XSCREEN (screen);

  application = g_object_new (VNCK_TYPE_APPLICATION, NULL);
  application->priv->xwindow = xwindow;
  application->priv->screen = screen;

  application->priv->name = _vnck_get_name (xscreen, xwindow);

  if (application->priv->name == NULL)
    application->priv->name = _vnck_get_res_class_utf8 (xscreen, xwindow);

  if (application->priv->name)
    application->priv->name_from_leader = TRUE;

  application->priv->pid = _vnck_get_pid (xscreen,
                                          application->priv->xwindow);

  application->priv->startup_id = _vnck_get_utf8_property (xscreen,
                                                           application->priv->xwindow,
                                                           _vnck_atom_get ("_NET_STARTUP_ID"));

  g_hash_table_insert (app_hash, &application->priv->xwindow, application);

  /* Hash now owns one ref, caller gets none */

  /* Note that xwindow may correspond to a VnckWindow's xwindow,
   * so we select events needed by either
   */
  application->priv->orig_event_mask = _vnck_select_input (xscreen,
                                                           application->priv->xwindow,
                                                           VNCK_APP_WINDOW_EVENT_MASK,
                                                           TRUE);

  return application;
}

void
_vnck_application_destroy (VnckApplication *application)
{
  Window xwindow = application->priv->xwindow;

  g_return_if_fail (vnck_application_get (xwindow) == application);

  g_hash_table_remove (app_hash, &xwindow);

  /* Removing from hash also removes the only ref VnckApplication had */

  g_return_if_fail (vnck_application_get (xwindow) == NULL);
}

static void
window_name_changed (VnckWindow      *window,
                     VnckApplication *app)
{
  if (window == app->priv->name_window)
    {
      reset_name (app);
      update_name (app);
    }
}

void
_vnck_application_add_window (VnckApplication *app,
                              VnckWindow      *window)
{
  g_return_if_fail (VNCK_IS_APPLICATION (app));
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (vnck_window_get_application (window) == NULL);

  app->priv->windows = g_list_prepend (app->priv->windows, window);
  _vnck_window_set_application (window, app);

  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed), app);

  /* emits signals, so do it last */
  reset_name (app);
  update_name (app);

  /* see if we're using icon from a window */
  if (app->priv->icon == NULL ||
      app->priv->mini_icon == NULL)
    emit_icon_changed (app);
}

void
_vnck_application_remove_window (VnckApplication *app,
                                 VnckWindow      *window)
{
  g_return_if_fail (VNCK_IS_APPLICATION (app));
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (vnck_window_get_application (window) == app);

  app->priv->windows = g_list_remove (app->priv->windows, window);
  _vnck_window_set_application (window, NULL);

  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        window_name_changed, app);

  /* emits signals, so do it last */
  reset_name (app);
  update_name (app);

  /* see if we're using icon from a window */
  if (app->priv->icon == NULL ||
      app->priv->mini_icon == NULL)
    emit_icon_changed (app);
}

void
_vnck_application_process_property_notify (VnckApplication *app,
                                           XEvent          *xevent)
{
  /* This prop notify is on the leader window */

  if (xevent->xproperty.atom == XA_WM_NAME ||
      xevent->xproperty.atom ==
      _vnck_atom_get ("_NET_WM_NAME") ||
      xevent->xproperty.atom ==
      _vnck_atom_get ("_NET_WM_VISIBLE_NAME"))
    {
      /* FIXME should change the name */
    }
  else if (xevent->xproperty.atom ==
           XA_WM_ICON_NAME ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_ICON_NAME") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"))
    {
      /* FIXME */
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_ICON") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("KWM_WIN_ICON") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("WM_NORMAL_HINTS"))
    {
      _vnck_icon_cache_property_changed (app->priv->icon_cache,
                                         xevent->xproperty.atom);
      emit_icon_changed (app);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_STARTUP_ID"))
    {
      /* FIXME update startup id */
    }
}

static void
emit_name_changed (VnckApplication *app)
{
  g_signal_emit (G_OBJECT (app),
                 signals[NAME_CHANGED],
                 0);
}

static void
emit_icon_changed (VnckApplication *app)
{
  app->priv->need_emit_icon_changed = FALSE;
  g_signal_emit (G_OBJECT (app),
                 signals[ICON_CHANGED],
                 0);
}

static void
reset_name  (VnckApplication *app)
{
  if (!app->priv->name_from_leader)
    {
      g_free (app->priv->name);
      app->priv->name = NULL;
      app->priv->name_window = NULL;
    }
}

static void
update_name (VnckApplication *app)
{
  g_assert (app->priv->name_from_leader || app->priv->name == NULL);

  if (app->priv->name == NULL)
    {
      /* if only one window, get name from there. If more than one and
       * they all have the same res_class, use that. Else we want to
       * use the fallback name, since using the title of one of the
       * windows would look wrong.
       */
      if (app->priv->windows &&
          app->priv->windows->next == NULL)
        {
          app->priv->name =
            g_strdup (vnck_window_get_name (app->priv->windows->data));
          app->priv->name_window = app->priv->windows->data;
          emit_name_changed (app);
        }
      else if (app->priv->windows)
        {
          /* more than one */
          app->priv->name =
            _vnck_get_res_class_utf8 (VNCK_SCREEN_XSCREEN (app->priv->screen),
                                      vnck_window_get_xid (app->priv->windows->data));
          if (app->priv->name)
            {
              app->priv->name_window = app->priv->windows->data;
              emit_name_changed (app);
            }
        }
    }
}
