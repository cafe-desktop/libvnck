/* window object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
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

#include <config.h>

#include <glib/gi18n-lib.h>
#include <string.h>
#include <time.h>

#include "window.h"
#include "class-group.h"
#include "util.h"
#include "xutils.h"
#include "private.h"
#include "vnck-enum-types.h"

/**
 * SECTION:window
 * @short_description: an object representing a window.
 * @see_also: #VnckWorkspace, #VnckApplication, #VnckClassGroup
 * @stability: Unstable
 *
 * The #VnckWindow objects are always owned by libvnck and must not be
 * referenced or unreferenced.
 */

#define FALLBACK_NAME _("Untitled window")
#define ALL_WORKSPACES ((int) 0xFFFFFFFF)

static GHashTable *window_hash = NULL;

/* Keep 0-7 in sync with the numbers in the WindowState enum. Yeah I'm
 * a loser.
 */
#define COMPRESS_STATE(window)                          \
  ( ((window)->priv->is_minimized        << 0) |        \
    ((window)->priv->is_maximized_horz   << 1) |        \
    ((window)->priv->is_maximized_vert   << 2) |        \
    ((window)->priv->is_shaded           << 3) |        \
    ((window)->priv->skip_pager          << 4) |        \
    ((window)->priv->skip_taskbar        << 5) |        \
    ((window)->priv->is_sticky           << 6) |        \
    ((window)->priv->is_hidden           << 7) |        \
    ((window)->priv->is_fullscreen       << 8) |        \
    ((window)->priv->demands_attention   << 9) |        \
    ((window)->priv->is_urgent           << 10)|        \
    ((window)->priv->is_above            << 11)|        \
    ((window)->priv->is_below            << 12))

struct _VnckWindowPrivate
{
  Window xwindow;
  VnckScreen *screen;
  VnckApplication *app;
  VnckClassGroup *class_group;
  Window group_leader;
  Window transient_for;
  int orig_event_mask;
  CdkRectangle icon_geometry;
  char *name;
  char *icon_name;
  char *session_id;
  char *session_id_utf8;
  char *role;
  int pid;
  int workspace;
  gint sort_order;

  VnckWindowType wintype;

  CdkPixbuf *icon;
  CdkPixbuf *mini_icon;

  VnckIconCache *icon_cache;

  VnckWindowActions actions;

  int x;
  int y;
  int width;
  int height;

  int left_frame;
  int right_frame;
  int top_frame;
  int bottom_frame;

  char *startup_id;

  char *res_class;
  char *res_name;

  /* true if transient_for points to root window,
   * not another app window
   */
  guint transient_for_root : 1;

  /* window state */
  guint is_minimized : 1;
  guint is_maximized_horz : 1;
  guint is_maximized_vert : 1;
  guint is_shaded : 1;
  guint is_above : 1;
  guint is_below : 1;
  guint skip_pager : 1;
  guint skip_taskbar : 1;
  guint is_sticky : 1;
  guint is_hidden : 1;
  guint is_fullscreen : 1;
  guint demands_attention : 1;
  guint is_urgent : 1;

  time_t needs_attention_time;

  /* _NET_WM_STATE_HIDDEN doesn't map directly into an
   * externally-visible state (it determines the WM_STATE
   * interpretation)
   */
  guint net_wm_state_hidden : 1;
  guint wm_state_iconic : 1;

  /* idle handler for updates */
  guint update_handler;

  /* if you add flags, be sure to set them
   * when we create the window so we get an initial update
   */
  guint need_update_name : 1;
  guint need_update_state : 1;
  guint need_update_wm_state : 1;
  guint need_update_icon_name : 1;
  guint need_update_workspace : 1;
  guint need_update_actions : 1;
  guint need_update_wintype : 1;
  guint need_update_transient_for : 1;
  guint need_update_startup_id : 1;
  guint need_update_wmclass : 1;
  guint need_update_wmhints : 1;
  guint need_update_frame_extents : 1;
  guint need_update_role : 1;

  guint need_emit_name_changed : 1;
  guint need_emit_icon_changed : 1;
  guint need_emit_class_changed : 1;
  guint need_emit_role_changed : 1;
  guint need_emit_type_changed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckWindow, vnck_window, G_TYPE_OBJECT);

enum {
  NAME_CHANGED,
  STATE_CHANGED,
  WORKSPACE_CHANGED,
  ICON_CHANGED,
  ACTIONS_CHANGED,
  GEOMETRY_CHANGED,
  CLASS_CHANGED,
  ROLE_CHANGED,
  TYPE_CHANGED,
  LAST_SIGNAL
};

static void vnck_window_finalize    (GObject        *object);

static void emit_name_changed      (VnckWindow      *window);
static void emit_state_changed     (VnckWindow      *window,
                                    VnckWindowState  changed_mask,
                                    VnckWindowState  new_state);
static void emit_workspace_changed (VnckWindow      *window);
static void emit_icon_changed      (VnckWindow      *window);
static void emit_actions_changed   (VnckWindow       *window,
                                    VnckWindowActions changed_mask,
                                    VnckWindowActions new_actions);
static void emit_geometry_changed  (VnckWindow      *window);
static void emit_class_changed     (VnckWindow      *window);
static void emit_role_changed      (VnckWindow      *window);
static void emit_type_changed      (VnckWindow      *window);

static void update_name      (VnckWindow *window);
static void update_state     (VnckWindow *window);
static void update_wm_state  (VnckWindow *window);
static void update_icon_name (VnckWindow *window);
static void update_workspace (VnckWindow *window);
static void update_actions   (VnckWindow *window);
static void update_wintype   (VnckWindow *window);
static void update_transient_for (VnckWindow *window);
static void update_startup_id (VnckWindow *window);
static void update_wmclass   (VnckWindow *window);
static void update_frame_extents (VnckWindow *window);
static void update_role      (VnckWindow *window);
static void unqueue_update   (VnckWindow *window);
static void queue_update     (VnckWindow *window);
static void force_update_now (VnckWindow *window);

static VnckWindow* find_last_transient_for (GList *windows,
                                            Window xwindow);

static guint signals[LAST_SIGNAL] = { 0 };

void
_vnck_window_shutdown_all (void)
{
  if (window_hash != NULL)
    {
      g_hash_table_destroy (window_hash);
      window_hash = NULL;
    }
}

static void
vnck_window_init (VnckWindow *window)
{
  window->priv = vnck_window_get_instance_private (window);

  window->priv->icon_cache = _vnck_icon_cache_new ();
  window->priv->icon_geometry.width = -1; /* invalid cached value */
  window->priv->workspace = -1;
  window->priv->sort_order = G_MAXINT;

  /* FIXME: should we have an invalid window type for this? */
  window->priv->wintype = VNCK_WINDOW_NORMAL;
}

static void
vnck_window_class_init (VnckWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vnck_window_finalize;

  /**
   * VnckWindow::name-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the name of @window changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::state-changed:
   * @window: the #VnckWindow which emitted the signal.
   * @changed_mask: the bitmask containing bits set for all states of @window
   * that have changed.
   * @new_state: the new state of @window.
   *
   * Emitted when the state of @window changes. This can happen when @window is
   * (un)minimized, (un)maximized, (un)sticked, (un)shaded, (un)made above,
   * (un)made below, (un)set fullscreen, when it needs attention, etc. See
   * #VnckWindowState for the complete list of states that might have changed.
   */
  signals[STATE_CHANGED] =
    g_signal_new ("state_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, state_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  VNCK_TYPE_WINDOW_STATE, VNCK_TYPE_WINDOW_STATE);

  /**
   * VnckWindow::workspace-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the current workspace of @window changes, or if @window has
   * been pinned or unpinned.
   */
  signals[WORKSPACE_CHANGED] =
    g_signal_new ("workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, workspace_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::icon-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the icon of @window changes.
   */
  signals[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::actions-changed:
   * @window: the #VnckWindow which emitted the signal.
   * @changed_mask: the bitmask containing bits set for all actions
   * availabilities for @window that have changed.
   * @new_state: the new actions availabilities for @window.
   *
   * Emitted when the actions availabilities for @window change.
   */
  signals[ACTIONS_CHANGED] =
    g_signal_new ("actions_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, actions_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  VNCK_TYPE_WINDOW_ACTIONS,
                  VNCK_TYPE_WINDOW_ACTIONS);

  /**
   * VnckWindow::geometry-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the geometry of @window changes.
   */
  signals[GEOMETRY_CHANGED] =
    g_signal_new ("geometry_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, geometry_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::class-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the class name or instance name of @window changes.
   */
  signals[CLASS_CHANGED] =
    g_signal_new ("class_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, class_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::role-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the role of @window changes.
   */
  signals[ROLE_CHANGED] =
    g_signal_new ("role_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, role_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * VnckWindow::type-changed:
   * @window: the #VnckWindow which emitted the signal.
   *
   * Emitted when the EWMH type hint of the window changes.
   *
   * Since: 3.20
   */
  signals[TYPE_CHANGED] =
    g_signal_new ("type_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWindowClass, type_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
vnck_window_finalize (GObject *object)
{
  VnckWindow *window;

  window = VNCK_WINDOW (object);

  _vnck_select_input (VNCK_SCREEN_XSCREEN (window->priv->screen),
                      window->priv->xwindow,
                      window->priv->orig_event_mask,
                      FALSE);

  unqueue_update (window);

  if (window->priv->app)
    g_object_unref (G_OBJECT (window->priv->app));
  window->priv->app = NULL;

  if (window->priv->class_group)
    g_object_unref (G_OBJECT (window->priv->class_group));
  window->priv->class_group = NULL;

  g_free (window->priv->name);
  window->priv->name = NULL;
  g_free (window->priv->icon_name);
  window->priv->icon_name = NULL;
  g_free (window->priv->session_id);
  window->priv->session_id = NULL;
  g_free (window->priv->session_id_utf8);
  window->priv->session_id_utf8 = NULL;

  if (window->priv->icon)
    g_object_unref (G_OBJECT (window->priv->icon));
  window->priv->icon = NULL;

  if (window->priv->mini_icon)
    g_object_unref (G_OBJECT (window->priv->mini_icon));
  window->priv->mini_icon = NULL;

  _vnck_icon_cache_free (window->priv->icon_cache);
  window->priv->icon_cache = NULL;

  g_free (window->priv->startup_id);
  window->priv->startup_id = NULL;
  g_free (window->priv->res_class);
  window->priv->res_class = NULL;
  g_free (window->priv->res_name);
  window->priv->res_name = NULL;

  window->priv->xwindow = None;

  G_OBJECT_CLASS (vnck_window_parent_class)->finalize (object);
}

/**
 * vnck_window_get:
 * @xwindow: an X window ID.
 *
 * Gets a preexisting #VnckWindow for the X window @xwindow. This will not
 * create a #VnckWindow if none exists. The function is robust against bogus
 * window IDs.
 *
 * Return value: (transfer none): the #VnckWindow for @xwindow. The returned
 * #VnckWindow is owned by libvnck and must not be referenced or unreferenced.
 **/
VnckWindow*
vnck_window_get (gulong xwindow)
{
  if (window_hash == NULL)
    return NULL;
  else
    return g_hash_table_lookup (window_hash, &xwindow);
}

/**
 * vnck_window_get_screen:
 * @window: a #VnckWindow.
 *
 * Gets the #VnckScreen @window is on.
 *
 * Return value: (transfer none): the #VnckScreen @window is on. The returned
 * #VnckScreen is owned by libvnck and must not be referenced or unreferenced.
 **/
VnckScreen*
vnck_window_get_screen (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->screen;
}

VnckWindow*
_vnck_window_create (Window      xwindow,
                     VnckScreen *screen,
                     gint        sort_order)
{
  VnckWindow *window;
  Screen     *xscreen;

  if (window_hash == NULL)
    window_hash = g_hash_table_new_full (_vnck_xid_hash, _vnck_xid_equal,
                                         NULL, g_object_unref);

  g_return_val_if_fail (g_hash_table_lookup (window_hash, &xwindow) == NULL,
                        NULL);

  xscreen = VNCK_SCREEN_XSCREEN (screen);

  window = g_object_new (VNCK_TYPE_WINDOW, NULL);
  window->priv->xwindow = xwindow;
  window->priv->screen = screen;

  g_hash_table_insert (window_hash, &window->priv->xwindow, window);

  /* Hash now owns one ref, caller gets none */

  /* Note that xwindow may correspond to a VnckApplication's xwindow,
   * that's why we select the union of the mask we want for Application
   * and the one we want for window
   */
  window->priv->orig_event_mask =_vnck_select_input (xscreen,
                                                     window->priv->xwindow,
                                                     VNCK_APP_WINDOW_EVENT_MASK,
                                                     TRUE);

  /* Default the group leader to the window itself; it is set in
   * update_wmhints() if a different group leader is specified.
   */
  window->priv->group_leader = window->priv->xwindow;

  window->priv->session_id =
    _vnck_get_session_id (xscreen, window->priv->xwindow);

  window->priv->pid =
    _vnck_get_pid (xscreen, window->priv->xwindow);

  window->priv->x = 0;
  window->priv->y = 0;
  window->priv->width = 0;
  window->priv->height = 0;
  _vnck_get_window_geometry (xscreen,
			     xwindow,
                             &window->priv->x,
                             &window->priv->y,
                             &window->priv->width,
                             &window->priv->height);

  window->priv->sort_order = sort_order;

  window->priv->need_update_name = TRUE;
  window->priv->need_update_state = TRUE;
  window->priv->need_update_icon_name = TRUE;
  window->priv->need_update_wm_state = TRUE;
  window->priv->need_update_workspace = TRUE;
  window->priv->need_update_actions = TRUE;
  window->priv->need_update_wintype = TRUE;
  window->priv->need_update_transient_for = TRUE;
  window->priv->need_update_startup_id = TRUE;
  window->priv->need_update_wmclass = TRUE;
  window->priv->need_update_wmhints = TRUE;
  window->priv->need_update_frame_extents = TRUE;
  window->priv->need_update_role = TRUE;
  window->priv->need_emit_name_changed = FALSE;
  window->priv->need_emit_icon_changed = FALSE;
  window->priv->need_emit_class_changed = FALSE;
  window->priv->need_emit_role_changed = FALSE;
  window->priv->need_emit_type_changed = FALSE;
  force_update_now (window);

  return window;
}

void
_vnck_window_destroy (VnckWindow *window)
{
  Window xwindow = window->priv->xwindow;

  g_return_if_fail (VNCK_IS_WINDOW (window));

  g_return_if_fail (vnck_window_get (xwindow) == window);

  g_hash_table_remove (window_hash, &xwindow);

  /* Removing from hash also removes the only ref VnckWindow had */

  g_return_if_fail (vnck_window_get (xwindow) == NULL);
}

static Display *
_vnck_window_get_display (VnckWindow *window)
{
  return DisplayOfScreen (VNCK_SCREEN_XSCREEN (window->priv->screen));
}

/**
 * vnck_window_has_name:
 * @window: a #VnckWindow.
 *
 * Checks whether or not @window has a name. vnck_window_get_name()
 * will always return some value, even if @window has no name set;
 * vnck_window_has_name() can be used to tell if that name is
 * real or not.
 *
 * For icons titles, use vnck_window_has_icon_name() instead.
 *
 * Return value: %TRUE if vnck_window_get_name() returns @window<!-- -->'s
 * name, %FALSE if it returns a fallback name.
 *
 * Since: 2.16
 **/
gboolean
vnck_window_has_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->name != NULL;
}

/**
 * vnck_window_get_name:
 * @window: a #VnckWindow.
 *
 * Gets the name of @window, as it should be displayed in a pager
 * or tasklist. Always returns some value, even if @window has no name
 * set; use vnck_window_has_name() if you need to know whether the returned
 * name is "real" or not.
 *
 * For icons titles, use vnck_window_get_icon_name() instead.
 *
 * Return value: the name of @window, or a fallback name if no name is
 * available.
 **/
const char*
vnck_window_get_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (window->priv->name)
    return window->priv->name;
  else
    return FALLBACK_NAME;
}

/**
 * vnck_window_has_icon_name:
 * @window: a #VnckWindow
 *
 * Checks whether or not @window has an icon name.
 * vnck_window_get_icon_name() will always return some value, even if
 * @window has no icon name set; vnck_window_has_icon_name() can
 * be used to tell if that icon name is real or not.
 *
 * (Note that if vnck_window_has_icon_name() returns %FALSE, but
 * vnck_window_has_name() returns %TRUE, then the name returned by
 * vnck_window_get_icon_name() is @window<!-- -->'s name. Only when both
 * methods return %FALSE does vnck_window_get_icon_name() return a
 * generic fallback name.)
 *
 * Return value: %TRUE if vnck_window_get_icon_name() returns
 * @window<!-- -->'s icon name, %FALSE if it returns a fallback name.
 *
 * Since: 2.16
 **/
gboolean
vnck_window_has_icon_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->icon_name != NULL;
}

/**
 * vnck_window_get_icon_name:
 * @window: a #VnckWindow
 *
 * Gets the icon name of @window, as it should be displayed for an icon
 * (minimized state). Always returns some value, even if @window has no icon
 * name set; use vnck_window_has_icon_name() if you need to know whether the
 * returned icon name is "real" or not.
 *
 * Contrast with vnck_window_get_name(), which returns @window<!-- -->'s
 * title, not its icon title.
 *
 * Return value: the icon name of @window, or a fallback icon name if no icon
 * name is available.
 **/
const char*
vnck_window_get_icon_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (window->priv->icon_name)
    return window->priv->icon_name;
  else if (window->priv->name)
    return window->priv->name;
  else
    return FALLBACK_NAME;
}

char *
_vnck_window_get_name_for_display (VnckWindow *window,
                                   gboolean    use_icon_name,
                                   gboolean    use_state_decorations)
{
  const char *name;

  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (use_icon_name && vnck_window_has_icon_name (window))
    name = vnck_window_get_icon_name (window);
  else
    name = vnck_window_get_name (window);

  if (use_state_decorations)
    {
      if (window->priv->is_shaded)
        return g_strdup_printf ("=%s=", name);
      else if (window->priv->is_minimized)
        return g_strdup_printf ("[%s]", name);
      else
        return g_strdup (name);
    }
  else
    return g_strdup (name);
}


/**
 * vnck_window_get_application:
 * @window: a #VnckWindow.
 *
 * Gets the #VnckApplication to which @window belongs.
 *
 * Return value: (transfer none): the #VnckApplication to which @window belongs.
 * The returned #VnckApplication is owned by libvnck and must not be referenced
 * or unreferenced.
 **/
VnckApplication*
vnck_window_get_application  (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->app;
}

/**
 * vnck_window_get_transient:
 * @window: a #VnckWindow.
 *
 * Gets the #VnckWindow for which @window is transient.
 *
 * Return value: (transfer none): the #VnckWindow for which @window is
 * transient, or %NULL if @window is not transient for any #VnckWindow.
 * The returned #VnckWindow is owned by libvnck and must not be referenced or
 * unreferenced.
 *
 * Since: 2.12
 **/
VnckWindow*
vnck_window_get_transient (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return vnck_window_get (window->priv->transient_for);
}

/**
 * vnck_window_get_group_leader:
 * @window: a #VnckWindow.
 *
 * Gets the group leader of the group of windows to which @window belongs.
 *
 * Return value: the group leader of the group of windows to which @window
 * belongs, or the X window ID of @window if @window does not belong to any
 * group.
 **/
gulong
vnck_window_get_group_leader (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), None);

  return window->priv->group_leader;
}

/**
 * vnck_window_get_xid:
 * @window: a #VnckWindow.
 *
 * Gets the X window ID of @window.
 *
 * Return value: the X window ID of @window.
 **/
gulong
vnck_window_get_xid (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), None);

  return window->priv->xwindow;
}

/**
 * vnck_window_get_class_group:
 * @window: a #VnckWindow.
 *
 * Gets the #VnckClassGroup to which @window belongs.
 *
 * Return value: (transfer none): the #VnckClassGroup to which @window belongs.
 * The returned #VnckClassGroup is owned by libvnck and must not be referenced
 * or unreferenced.
 *
 * Since: 2.2
 **/
VnckClassGroup *
vnck_window_get_class_group (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->class_group;
}

/**
 * vnck_window_get_session_id:
 * @window: a #VnckWindow.
 *
 * Gets the session ID for @window in Latin-1 encoding.
 * NOTE: this is invalid UTF-8. You can't display this
 * string in a CTK+ widget without converting to UTF-8.
 * See vnck_window_get_session_id_utf8().
 *
 * Return value: the session ID for @window in Latin-1, or %NULL if @window has
 * no session ID.
 **/
const char*
vnck_window_get_session_id (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->session_id;
}

/**
 * vnck_window_get_session_id_utf8:
 * @window: a #VnckWindow.
 *
 * Gets the session ID for @window in UTF-8 encoding.
 * The session ID should be in Latin-1 encoding, so the conversion should work,
 * but a broken client could set a session ID that might not be convertable to
 * UTF-8.
 *
 * Return value: the session ID for @window in UTF-8, or %NULL if @window has
 * no session ID.
 **/
const char*
vnck_window_get_session_id_utf8 (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (window->priv->session_id_utf8 == NULL &&
      window->priv->session_id != NULL)
    {
      GString *str;
      char *p;

      str = g_string_new ("");

      p = window->priv->session_id;
      while (*p)
        {
          g_string_append_unichar (str, g_utf8_get_char (p));
          p = g_utf8_next_char (p);
        }

      window->priv->session_id_utf8 = g_string_free (str, FALSE);
    }

  return window->priv->session_id_utf8;
}

/**
 * vnck_window_get_role:
 * @window: a #VnckWindow.
 *
 * Gets the role for @window.
 * The role uniquely identifies a window among all windows that have the same
 * client leader window.
 *
 * Return value: role for @window, or %NULL if @window has no role.
 **/
const char*
vnck_window_get_role (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->role;
}

/**
 * vnck_window_get_pid:
 * @window: a #VnckWindow.
 *
 * Gets the process ID of @window.
 *
 * Return value: the process ID of @window, or 0 if none is available.
 **/
int
vnck_window_get_pid (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  return window->priv->pid;
}

/**
 * vnck_window_get_sort_order:
 * @window: a #VnckWindow.
 *
 * Gets the sort order of @window, used for ordering of @window in
 * #VnckSelector and #VnckTasklist. The sort order is an internal state in
 * libvnck. The initial value is defined when the window is created.
 *
 * Return value: the sort order of @window, or G_MAXINT if none is available.
 *
 * Since: 2.10
 **/
gint
vnck_window_get_sort_order (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), G_MAXINT);

  return window->priv->sort_order;
}

/**
 * vnck_window_set_sort_order:
 * @window: a #VnckWindow.
 * @order: new sort order for @window.
 *
 * Sets the sort order of @window. The sort order is used for ordering of
 * @window in #VnckSelector and #VnckTasklist.
 *
 * Since: 2.20
 **/
void        vnck_window_set_sort_order        (VnckWindow *window,
					       gint order)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  window->priv->sort_order = order;
  return;
}

/**
 * vnck_window_get_window_type:
 * @window: a #VnckWindow.
 *
 * Gets the semantic type of @window.
 *
 * Return value: the semantic type of @window.
 **/
VnckWindowType
vnck_window_get_window_type (VnckWindow *window)
{
  /* FIXME: should we have an invalid window type for this? */
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  return window->priv->wintype;
}

/**
 * vnck_window_set_window_type:
 * @window: a #VnckWindow.
 * @wintype: a semantic type.
 *
 * Sets the semantic type of @window to @wintype.
 *
 * Since: 2.12
 **/
void
vnck_window_set_window_type (VnckWindow *window, VnckWindowType wintype)
{
  Atom atom;
  Display *display;

  g_return_if_fail (VNCK_IS_WINDOW (window));

  switch (wintype) {
  case VNCK_WINDOW_NORMAL:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_NORMAL");
    break;
  case VNCK_WINDOW_DESKTOP:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DESKTOP");
    break;
  case VNCK_WINDOW_DOCK:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DOCK");
    break;
  case VNCK_WINDOW_DIALOG:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DIALOG");
    break;
  case VNCK_WINDOW_TOOLBAR:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_TOOLBAR");
    break;
  case VNCK_WINDOW_MENU:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_MENU");
    break;
  case VNCK_WINDOW_UTILITY:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_UTILITY");
    break;
  case VNCK_WINDOW_SPLASHSCREEN:
    atom = _vnck_atom_get ("_NET_WM_WINDOW_TYPE_SPLASH");
    break;
  default:
    return;
  }

  display = _vnck_window_get_display (window);

  _vnck_error_trap_push (display);

  XChangeProperty (display,
                   window->priv->xwindow,
                   _vnck_atom_get ("_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);

  _vnck_error_trap_pop (display);

  emit_type_changed (window);
}

/**
 * vnck_window_is_minimized:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is minimized. Minimization state may change anytime
 * a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is minimized, %FALSE otherwise.
 **/
gboolean
vnck_window_is_minimized (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_minimized;
}

/**
 * vnck_window_needs_attention:
 * @window: a #VnckWindow.
 *
 * Gets whether @window needs attention. This state may change anytime
 * a #VnckWindow::state-changed signal gets emitted.
 *
 * This state depends on flags such as the demands_attention and is_urgent
 * hints.
 *
 * Return value: %TRUE if @window needs attention, %FALSE otherwise.
 *
 * Since: 2.12
 **/
gboolean
vnck_window_needs_attention (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->demands_attention || window->priv->is_urgent;
}

time_t
_vnck_window_get_needs_attention_time (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  return window->priv->needs_attention_time;
}

/* Return whether the transient of @window needs attention */
static VnckWindow *
transient_needs_attention (VnckWindow *window)
{
  GList *windows;
  VnckWindow *transient;

  if (!VNCK_IS_WINDOW (window))
    return NULL;

  windows = vnck_screen_get_windows_stacked (window->priv->screen);

  transient = window;
  while ((transient = find_last_transient_for (windows, transient->priv->xwindow)))
    {
      /* catch transient cycles */
      if (transient == window)
        return NULL;

      if (vnck_window_needs_attention (transient))
        return transient;
    }

  return FALSE;
}

time_t
_vnck_window_or_transient_get_needs_attention_time (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  if (_vnck_window_get_needs_attention_time (window) == 0)
    {
      VnckWindow *transient;

      transient = transient_needs_attention (window);
      if (transient)
        return _vnck_window_get_needs_attention_time (transient);
      else
        return 0;
    }
  else
    return _vnck_window_get_needs_attention_time (window);
}

/**
 * vnck_window_or_transient_needs_attention:
 * @window: a #VnckWindow.
 *
 * Gets whether @window or one of its transients needs attention. This state
 * may change anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window or one of its transients needs attention,
 * %FALSE otherwise.
 *
 * Since: 2.12
 **/
gboolean
vnck_window_or_transient_needs_attention (VnckWindow *window)
{
  return vnck_window_needs_attention (window) ||
         transient_needs_attention (window) != NULL;
}

/**
 * vnck_window_is_maximized_horizontally:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is maximized horizontally. Horizontal maximization
 * state may change anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is maximized horizontally, %FALSE otherwise.
 **/
gboolean
vnck_window_is_maximized_horizontally (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_maximized_horz;
}

/**
 * vnck_window_is_maximized_vertically:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is maximized vertically. vertiVal maximization
 * state may change anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is maximized vertically, %FALSE otherwise.
 **/
gboolean
vnck_window_is_maximized_vertically   (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_maximized_vert;
}

const char*
_vnck_window_get_startup_id (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (window->priv->startup_id == NULL &&
      window->priv->group_leader != None)
    {
      VnckApplication *app;

      /* Fall back to group leader property */

      app = vnck_application_get (window->priv->group_leader);

      if (app != NULL)
        return vnck_application_get_startup_id (app);
      else
        return NULL;
    }

  return window->priv->startup_id;
}

/**
 * vnck_window_get_class_group_name:
 * @window: a #VnckWindow.
 *
 * Gets the class group name from the <ulink
 * url="http://tronche.com/gui/x/icccm/sec-4.html&num;WM_CLASS">WM_CLASS Property</ulink>
 * for @window.
 *
 * The class group name is also the identifier name of the #VnckClassGroup to
 * which @window belongs.
 *
 * Return value: the class group name for @window, or %NULL if @window belongs
 * to no class group.
 **/
const char*
vnck_window_get_class_group_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->res_class;
}

/**
 * vnck_window_get_class_instance_name:
 * @window: a #VnckWindow.
 *
 * Gets the class instance name from the <ulink
 * url="http://tronche.com/gui/x/icccm/sec-4.html&num;WM_CLASS">WM_CLASS Property</ulink>
 * for @window.
 *
 * The class instance name allows to differentiate windows belonging to the
 * same class group, so that they can use different resources.
 *
 * Return value: the class instance name for @window, or %NULL if @window has
 * no class instance.
 **/
const char*
vnck_window_get_class_instance_name (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  return window->priv->res_name;
}

/**
 * vnck_window_is_maximized:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is maximized. Maximization state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * As for CDK, "maximized" means both vertically and horizontally. If @window
 * is maximized in only one direction, then @window is not considered
 * maximized.
 *
 * Return value: %TRUE if @window is maximized in both directions, %FALSE
 * otherwise.
 **/
gboolean
vnck_window_is_maximized (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return
    window->priv->is_maximized_horz &&
    window->priv->is_maximized_vert;
}

/**
 * vnck_window_is_shaded:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is shaded. Shade state may change anytime
 * a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is shaded, %FALSE otherwise.
 **/
gboolean
vnck_window_is_shaded                 (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_shaded;
}

/**
 * vnck_window_is_above:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is above other windows. This state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * See vnck_window_make_above() for more details on this state.
 *
 * Return value: %TRUE if @window is above other windows, %FALSE otherwise.
 *
 * Since: 2.14
 **/
gboolean
vnck_window_is_above                  (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_above;
}

/**
 * vnck_window_is_below:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is below other windows. This state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * See vnck_window_make_below() for more details on this state.
 *
 * Return value: %TRUE if @window is below other windows, %FALSE otherwise.
 *
 * Since: 2.20
 **/
gboolean
vnck_window_is_below                  (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_below;
}

/**
 * vnck_window_is_skip_pager:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is included on pagers. This state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is included on pagers, %FALSE otherwise.
 **/
gboolean
vnck_window_is_skip_pager             (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->skip_pager;
}

/**
 * vnck_window_set_skip_pager:
 * @window: a #VnckWindow.
 * @skip: whether @window should be included on pagers.
 *
 * Asks the window manager to make @window included or not included on pagers.
 **/
void
vnck_window_set_skip_pager (VnckWindow *window,
                            gboolean    skip)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      skip,
                      _vnck_atom_get ("_NET_WM_STATE_SKIP_PAGER"),
                      0);
}

/**
 * vnck_window_is_skip_tasklist:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is included on tasklists. This state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is included on tasklists, %FALSE otherwise.
 **/
gboolean
vnck_window_is_skip_tasklist          (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->skip_taskbar;
}

/**
 * vnck_window_is_fullscreen:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is fullscreen. Fullscreen state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Return value: %TRUE if @window is fullscreen, %FALSE otherwise.
 *
 * Since: 2.8
 **/
gboolean
vnck_window_is_fullscreen                 (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_fullscreen;
}

/**
 * vnck_window_set_skip_tasklist:
 * @window: a #VnckWindow.
 * @skip: whether @window should be included on tasklists.
 *
 * Asks the window manager to make @window included or not included on
 * tasklists.
 **/
void
vnck_window_set_skip_tasklist (VnckWindow *window,
                               gboolean    skip)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      skip,
                      _vnck_atom_get ("_NET_WM_STATE_SKIP_TASKBAR"),
                      0);
}

/**
 * vnck_window_set_fullscreen:
 * @window: a #VnckWindow.
 * @fullscreen: whether to make @window fullscreen.
 *
 * Asks the window manager to set the fullscreen state of @window according to
 * @fullscreen.
 *
 * Since: 2.8
 **/
void
vnck_window_set_fullscreen (VnckWindow *window,
                            gboolean    fullscreen)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      fullscreen,
                      _vnck_atom_get ("_NET_WM_STATE_FULLSCREEN"),
                      0);
}

/**
 * vnck_window_is_sticky:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is sticky. Sticky state may change
 * anytime a #VnckWindow::state-changed signal gets emitted.
 *
 * Sticky here means "stuck to the glass", i.e. does not scroll with the
 * viewport. In CDK/CTK+ (e.g. cdk_window_stick()/ctk_window_stick()), sticky
 * means "stuck to the glass" and <emphasis>also</emphasis> that the window is
 * on all workspaces. But here it only means the viewport aspect of it.
 *
 * Return value: %TRUE if @window is "stuck to the glass", %FALSE otherwise.
 **/
gboolean
vnck_window_is_sticky                 (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->is_sticky;
}

/**
 * vnck_window_close:
 * @window: a #VnckWindow.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 *
 * Closes @window.
 *
 * This function existed before 2.6, but the @timestamp argument was missing
 * in earlier versions.
 *
 * Since: 2.6
 **/
void
vnck_window_close (VnckWindow *window,
                   guint32     timestamp)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_close (window->priv->screen, window->priv->xwindow, timestamp);
}

/**
 * vnck_window_minimize:
 * @window: a #VnckWindow.
 *
 * Minimizes @window.
 **/
void
vnck_window_minimize                (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_iconify (VNCK_SCREEN_XSCREEN (window->priv->screen),
                 window->priv->xwindow);
}

/**
 * vnck_window_unminimize:
 * @window: a #VnckWindow.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 *
 * Unminimizes @window by activating it or one of its transients. See
 * vnck_window_activate_transient() for details on how the activation is done.
 **/
void
vnck_window_unminimize              (VnckWindow *window,
                                     guint32     timestamp)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  vnck_window_activate_transient (window, timestamp);
}

/**
 * vnck_window_maximize:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to maximize @window.
 **/
void
vnck_window_maximize (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

/**
 * vnck_window_unmaximize:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to unmaximize @window.
 **/
void
vnck_window_unmaximize (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

/**
 * vnck_window_maximize_horizontally:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to maximize horizontally @window.
 **/
void
vnck_window_maximize_horizontally (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"),
                      0);
}

/**
 * vnck_window_unmaximize_horizontally:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to unmaximize horizontally @window.
 **/
void
vnck_window_unmaximize_horizontally (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"),
                      0);
}

/**
 * vnck_window_maximize_vertically:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to maximize vertically @window.
 **/
void
vnck_window_maximize_vertically (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      0);
}

/**
 * vnck_window_unmaximize_vertically:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to unmaximize vertically @window.
 **/
void
vnck_window_unmaximize_vertically (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"),
                      0);
}

/**
 * vnck_window_shade:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to shade @window.
 **/
void
vnck_window_shade (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_SHADED"),
                      0);
}

/**
 * vnck_window_unshade:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to unshade @window.
 **/
void
vnck_window_unshade (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_SHADED"),
                      0);
}

/**
 * vnck_window_make_above:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to put @window on top of most windows (@window will
 * not be on top of focused fullscreen windows, of other windows with this
 * setting and of dock windows).
 *
 * Since: 2.14
 **/
void
vnck_window_make_above (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_ABOVE"),
                      0);
}

/**
 * vnck_window_unmake_above:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to not put @window on top of most windows, and to
 * put it again in the stack with other windows.
 *
 * Since: 2.14
 **/
void
vnck_window_unmake_above (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_ABOVE"),
                      0);
}

/**
 * vnck_window_make_below:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to put @window below most windows.
 *
 * Since: 2.20
 **/
void
vnck_window_make_below (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_BELOW"),
                      0);
}

/**
 * vnck_window_unmake_below:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to not put @window below most windows, and to
 * put it again in the stack with other windows.
 *
 * Since: 2.20
 **/
void
vnck_window_unmake_below (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_BELOW"),
                      0);
}

/**
 * vnck_window_stick:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to keep the @window<!-- -->'s position fixed on the
 * screen, even when the workspace or viewport scrolls.
 **/
void
vnck_window_stick (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      TRUE,
                      _vnck_atom_get ("_NET_WM_STATE_STICKY"),
                      0);
}

/**
 * vnck_window_unstick:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to not have @window<!-- -->'s position fixed on the
 * screen when the workspace or viewport scrolls.
 **/
void
vnck_window_unstick (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_state (window->priv->screen,
                      window->priv->xwindow,
                      FALSE,
                      _vnck_atom_get ("_NET_WM_STATE_STICKY"),
                      0);
}

/**
 * vnck_window_keyboard_move:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to start moving @window via the keyboard.
 **/
void
vnck_window_keyboard_move (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_keyboard_move (window->priv->screen,
                       window->priv->xwindow);
}

/**
 * vnck_window_keyboard_size:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to start resizing @window via the keyboard.
 **/
void
vnck_window_keyboard_size (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_keyboard_size (window->priv->screen,
                       window->priv->xwindow);
}

/**
 * vnck_window_get_workspace:
 * @window: a #VnckWindow.
 *
 * Gets the current workspace @window is on. If the window is pinned (on all
 * workspaces), or not on any workspaces, %NULL may be returned.
 *
 * Return value: (transfer none): the single current workspace @window is on, or
 * %NULL. The returned #VnckWorkspace is owned by libvnck and must not be
 * referenced or unreferenced.
 **/
VnckWorkspace*
vnck_window_get_workspace (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  if (window->priv->workspace == ALL_WORKSPACES)
    return NULL;
  else
    return vnck_screen_get_workspace (window->priv->screen, window->priv->workspace);
}

/**
 * vnck_window_move_to_workspace:
 * @window: a #VnckWindow.
 * @space: a #VnckWorkspace.
 *
 * Asks the window manager to move @window to @space. If @window was pinned, it
 * will also result in @window being visible only on @space.
 **/
void
vnck_window_move_to_workspace (VnckWindow    *window,
                               VnckWorkspace *space)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (VNCK_IS_WORKSPACE (space));

  _vnck_change_workspace (window->priv->screen,
                          window->priv->xwindow,
                          vnck_workspace_get_number (space));
}

/**
 * vnck_window_is_pinned:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is on all workspace. Pinned state may change
 * anytime a #VnckWindow::workspace-changed signal gets emitted, but not when
 * a #VnckWindow::state-changed gets emitted.
 *
 * Return value: %TRUE if @window is on all workspaces, %FALSE otherwise.
 **/
gboolean
vnck_window_is_pinned (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window->priv->workspace == ALL_WORKSPACES;
}

/**
 * vnck_window_pin:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to put @window on all workspaces.
 **/
void
vnck_window_pin (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_change_workspace (window->priv->screen,
                          window->priv->xwindow,
                          ALL_WORKSPACES);
}

/**
 * vnck_window_unpin:
 * @window: a #VnckWindow.
 *
 * Asks the window manager to put @window only in the currently active
 * workspace, if @window was previously pinned. If @window was not pinned,
 * does not change @window<!-- -->'s workspace. If the active workspace
 * is not known for some reason (it should not happen much), sets
 * @window<!-- -->'s workspace to the first workspace.
 **/
void
vnck_window_unpin (VnckWindow *window)
{
  VnckWorkspace *active;

  g_return_if_fail (VNCK_IS_WINDOW (window));

  if (window->priv->workspace != ALL_WORKSPACES)
    return;

  active = vnck_screen_get_active_workspace (window->priv->screen);

  _vnck_change_workspace (window->priv->screen,
                          window->priv->xwindow,
                          active ? vnck_workspace_get_number (active) : 0);
}

/**
 * vnck_window_activate:
 * @window: a #VnckWindow.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 *
 * Asks the window manager to make @window the active window. The
 * window manager may choose to raise @window along with focusing it, and may
 * decide to refuse the request (to not steal the focus if there is a more
 * recent user activity, for example).
 *
 * This function existed before 2.10, but the @timestamp argument was missing
 * in earlier versions.
 *
 * Since: 2.10
 **/
void
vnck_window_activate (VnckWindow *window,
                      guint32     timestamp)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  _vnck_activate (window->priv->screen,
                  window->priv->xwindow,
                  timestamp);
}

/**
 * vnck_window_is_active:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is the active window on its #VnckScreen.
 *
 * Return value: %TRUE if @window is the active window on its #VnckScreen,
 * %FALSE otherwise.
 **/
gboolean
vnck_window_is_active (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return window == vnck_screen_get_active_window (window->priv->screen);
}

/**
 * vnck_window_is_most_recently_activated:
 * @window: a #VnckWindow.
 *
 * Gets whether @window is the most recently activated window on its
 * #VnckScreen.
 *
 * The most recently activated window is identical to the active
 * window for click and sloppy focus methods (since a window is always
 * active in those cases) but differs slightly for mouse focus since
 * there often is no active window.
 *
 * Return value: %TRUE if @window was the most recently activated window on its
 * #VnckScreen, %FALSE otherwise.
 *
 * Since: 2.8
 **/
gboolean
vnck_window_is_most_recently_activated (VnckWindow *window)
{
  VnckWindow * current;
  VnckWindow * previous;
  VnckWindow * most_recently_activated_window;

  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  current  = vnck_screen_get_active_window (window->priv->screen);
  previous = vnck_screen_get_previously_active_window (window->priv->screen);

  if (current)
    most_recently_activated_window = current;
  else
    most_recently_activated_window = previous;

  return (window == most_recently_activated_window);
}

static VnckWindow*
find_last_transient_for (GList *windows,
                         Window xwindow)
{
  GList *tmp;
  VnckWindow *retval;

  /* find _last_ transient for xwindow in the list */

  retval = NULL;

  tmp = windows;
  while (tmp != NULL)
    {
      VnckWindow *w = tmp->data;

      if (w->priv->transient_for == xwindow &&
	  w->priv->wintype != VNCK_WINDOW_UTILITY)
        retval = w;

      tmp = tmp->next;
    }

  return retval;
}

/**
 * vnck_window_activate_transient:
 * @window: a #VnckWindow.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 *
 * If @window has transients, activates the most likely transient
 * instead of the window itself. Otherwise activates @window.
 *
 * FIXME the ideal behavior of this function is probably to activate
 * the most recently active window among @window and its transients.
 * This is probably best implemented on the window manager side.
 *
 * This function existed before 2.10, but the @timestamp argument was missing
 * in earlier versions.
 *
 * Since: 2.10
 **/
void
vnck_window_activate_transient (VnckWindow *window,
                                guint32     timestamp)
{
  GList *windows;
  VnckWindow *transient;
  VnckWindow *next;

  g_return_if_fail (VNCK_IS_WINDOW (window));

  windows = vnck_screen_get_windows_stacked (window->priv->screen);

  transient = NULL;
  next = find_last_transient_for (windows, window->priv->xwindow);

  while (next != NULL)
    {
      if (next == window)
        {
          /* catch transient cycles */
          transient = NULL;
          break;
        }

      transient = next;

      next = find_last_transient_for (windows, transient->priv->xwindow);
    }

  if (transient != NULL)
    vnck_window_activate (transient, timestamp);
  else
    vnck_window_activate (window, timestamp);
}

/**
 * vnck_window_transient_is_most_recently_activated:
 * @window: a #VnckWindow.
 *
 * Gets whether one of the transients of @window is the most
 * recently activated window. See
 * vnck_window_is_most_recently_activated() for a more complete
 * description of what is meant by most recently activated.  This
 * function is needed because clicking on a #VnckTasklist once will
 * activate a transient instead of @window itself
 * (vnck_window_activate_transient), and clicking again should
 * minimize @window and its transients.  (Not doing this can be
 * especially annoying in the case of modal dialogs that don't appear
 * in the #VnckTasklist).
 *
 * Return value: %TRUE if one of the transients of @window is the most recently
 * activated window, %FALSE otherwise.
 *
 * Since: 2.12
 **/
gboolean
vnck_window_transient_is_most_recently_activated (VnckWindow *window)
{
  GList *windows;
  VnckWindow *transient;

  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  windows = vnck_screen_get_windows_stacked (window->priv->screen);

  transient = window;
  while ((transient = find_last_transient_for (windows, transient->priv->xwindow)))
    {
      /* catch transient cycles */
      if (transient == window)
        return FALSE;

      if (vnck_window_is_most_recently_activated (transient))
        return TRUE;
    }

  return FALSE;
}

static void
get_icons (VnckWindow *window)
{
  CdkPixbuf *icon;
  CdkPixbuf *mini_icon;
  gsize normal_size;
  gsize mini_size;

  icon = NULL;
  mini_icon = NULL;
  normal_size = _vnck_get_default_icon_size ();
  mini_size = _vnck_get_default_mini_icon_size ();

  if (_vnck_read_icons (window->priv->screen,
                        window->priv->xwindow,
                        window->priv->icon_cache,
                        &icon, normal_size, normal_size,
                        &mini_icon, mini_size, mini_size))
    {
      window->priv->need_emit_icon_changed = TRUE;

      if (window->priv->icon)
        g_object_unref (G_OBJECT (window->priv->icon));

      if (window->priv->mini_icon)
        g_object_unref (G_OBJECT (window->priv->mini_icon));

      window->priv->icon = icon;
      window->priv->mini_icon = mini_icon;
    }

  g_assert ((window->priv->icon && window->priv->mini_icon) ||
            !(window->priv->icon || window->priv->mini_icon));
}

void
_vnck_window_load_icons (VnckWindow *window)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  get_icons (window);
  if (window->priv->need_emit_icon_changed)
    queue_update (window); /* not done in get_icons since we call that from
                            * the update
                            */
}

/**
 * vnck_window_get_icon:
 * @window: a #VnckWindow.
 *
 * Gets the icon to be used for @window. If no icon was found, a fallback
 * icon is used. vnck_window_get_icon_is_fallback() can be used to tell if the
 * icon is the fallback icon.
 *
 * Return value: (transfer none): the icon for @window. The caller should
 * reference the returned <classname>CdkPixbuf</classname> if it needs to keep
 * the icon around.
 **/
CdkPixbuf*
vnck_window_get_icon (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  _vnck_window_load_icons (window);

  return window->priv->icon;
}

/**
 * vnck_window_get_mini_icon:
 * @window: a #VnckWindow.
 *
 * Gets the mini-icon to be used for @window. If no mini-icon was found, a
 * fallback mini-icon is used. vnck_window_get_icon_is_fallback() can be used
 * to tell if the mini-icon is the fallback mini-icon.
 *
 * Return value: (transfer none): the mini-icon for @window. The caller should
 * reference the returned <classname>CdkPixbuf</classname> if it needs to keep
 * the icon around.
 **/
CdkPixbuf*
vnck_window_get_mini_icon (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), NULL);

  _vnck_window_load_icons (window);

  return window->priv->mini_icon;
}

/**
 * vnck_window_get_icon_is_fallback:
 * @window: a #VnckWindow.
 *
 * Gets whether a default fallback icon is used for @window (because none
 * was set on @window).
 *
 * Return value: %TRUE if the icon for @window is a fallback, %FALSE otherwise.
 **/
gboolean
vnck_window_get_icon_is_fallback (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);

  return _vnck_icon_cache_get_is_fallback (window->priv->icon_cache);
}

/**
 * vnck_window_get_actions:
 * @window: a #VnckWindow.
 *
 * Gets the actions that can be done for @window.
 *
 * Return value: bitmask of actions that can be done for @window.
 **/
VnckWindowActions
vnck_window_get_actions (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  return window->priv->actions;
}


/**
 * vnck_window_get_state:
 * @window: a #VnckWindow.
 *
 * Gets the state of @window.
 *
 * Return value: bitmask of active states for @window.
 **/
VnckWindowState
vnck_window_get_state (VnckWindow *window)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), 0);

  return COMPRESS_STATE (window);
}

/**
 * vnck_window_get_client_window_geometry:
 * @window: a #VnckWindow.
 * @xp: (out): return location for X coordinate in pixels of @window.
 * @yp: (out): return location for Y coordinate in pixels of @window.
 * @widthp: (out): return location for width in pixels of @window.
 * @heightp: (out): return location for height in pixels of @window.
 *
 * Gets the size and position of @window, as last received
 * in a ConfigureNotify event (i.e. this call does not round-trip
 * to the server, just gets the last size we were notified of).
 * The X and Y coordinates are relative to the root window.
 *
 * The window manager usually adds a frame around windows. If
 * you need to know the size of @window with the frame, use
 * vnck_window_get_geometry().
 *
 * Since: 2.20
 **/
void
vnck_window_get_client_window_geometry (VnckWindow *window,
                                        int        *xp,
					int        *yp,
					int        *widthp,
					int        *heightp)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  if (xp)
    *xp = window->priv->x;
  if (yp)
    *yp = window->priv->y;
  if (widthp)
    *widthp = window->priv->width;
  if (heightp)
    *heightp = window->priv->height;
}

/**
 * vnck_window_get_geometry:
 * @window: a #VnckWindow.
 * @xp: (out): return location for X coordinate in pixels of @window.
 * @yp: (out): return location for Y coordinate in pixels of @window.
 * @widthp: (out): return location for width in pixels of @window.
 * @heightp: (out): return location for height in pixels of @window.
 *
 * Gets the size and position of @window, including decorations. This
 * function uses the information last received in a ConfigureNotify
 * event and adjusts it according to the size of the frame that is
 * added by the window manager (this call does not round-trip to the
 * server, it just gets the last sizes that were notified). The
 * X and Y coordinates are relative to the root window.
 *
 * If you need to know the actual size of @window ignoring the frame
 * added by the window manager, use vnck_window_get_client_window_geometry().
 **/
void
vnck_window_get_geometry (VnckWindow *window,
                          int        *xp,
                          int        *yp,
                          int        *widthp,
                          int        *heightp)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  if (xp)
    *xp = window->priv->x - window->priv->left_frame;
  if (yp)
    *yp = window->priv->y - window->priv->top_frame;
  if (widthp)
    *widthp = window->priv->width + window->priv->left_frame + window->priv->right_frame;
  if (heightp)
    *heightp = window->priv->height + window->priv->top_frame + window->priv->bottom_frame;
}

/**
 * vnck_window_set_geometry:
 * @window: a #VnckWindow.
 * @gravity: the gravity point to use as a reference for the new position.
 * @geometry_mask: a bitmask containing flags for what should be set.
 * @x: new X coordinate in pixels of @window.
 * @y: new Y coordinate in pixels of @window.
 * @width: new width in pixels of @window.
 * @height: new height in pixels of @window.
 *
 * Sets the size and position of @window. The X and Y coordinates should be
 * relative to the root window.
 *
 * Note that the new size and position apply to @window with its frame added
 * by the window manager. Therefore, using vnck_window_set_geometry() with
 * the values returned by vnck_window_get_geometry() should be a no-op, while
 * using vnck_window_set_geometry() with the values returned by
 * vnck_window_get_client_window_geometry() should reduce the size of @window
 * and move it.
 *
 * Since: 2.16
 **/
void
vnck_window_set_geometry (VnckWindow               *window,
                          VnckWindowGravity         gravity,
                          VnckWindowMoveResizeMask  geometry_mask,
                          int                       x,
                          int                       y,
                          int                       width,
                          int                       height)
{
  int gravity_and_flags;
  int source;

  g_return_if_fail (VNCK_IS_WINDOW (window));

  source = _vnck_get_client_type();
  gravity_and_flags = gravity;
  gravity_and_flags |= geometry_mask << 8;
  gravity_and_flags |= source << 12;

  x += window->priv->left_frame;
  y += window->priv->top_frame;
  width -= window->priv->left_frame + window->priv->right_frame;
  height -= window->priv->top_frame + window->priv->bottom_frame;

  _vnck_set_window_geometry (VNCK_SCREEN_XSCREEN (window->priv->screen),
                             window->priv->xwindow,
                             gravity_and_flags, x, y, width, height);
}

/**
 * vnck_window_is_visible_on_workspace:
 * @window: a #VnckWindow.
 * @workspace: a #VnckWorkspace.
 *
 * Like vnck_window_is_on_workspace(), but also checks that
 * the window is in a visible state (i.e. not minimized or shaded).
 *
 * Return value: %TRUE if @window appears on @workspace in normal state, %FALSE
 * otherwise.
 **/
gboolean
vnck_window_is_visible_on_workspace (VnckWindow    *window,
                                     VnckWorkspace *workspace)
{
  VnckWindowState state;

  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (VNCK_IS_WORKSPACE (workspace), FALSE);

  state = vnck_window_get_state (window);

  if (state & VNCK_WINDOW_STATE_HIDDEN)
    return FALSE; /* not visible */

  return vnck_window_is_on_workspace (window, workspace);
}

/**
 * vnck_window_set_icon_geometry:
 * @window: a #VnckWindow.
 * @x: X coordinate in pixels.
 * @y: Y coordinate in pixels.
 * @width: width in pixels.
 * @height: height in pixels.
 *
 * Sets the icon geometry for @window. A typical use case for this is the
 * destination of the minimization animation of @window.
 */
void
vnck_window_set_icon_geometry (VnckWindow *window,
			       int         x,
			       int         y,
			       int         width,
			       int         height)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));

  if (window->priv->icon_geometry.x == x &&
      window->priv->icon_geometry.y == y &&
      window->priv->icon_geometry.width == width &&
      window->priv->icon_geometry.height == height)
    return;

  window->priv->icon_geometry.x = x;
  window->priv->icon_geometry.y = y;
  window->priv->icon_geometry.width = width;
  window->priv->icon_geometry.height = height;

  _vnck_set_icon_geometry (VNCK_SCREEN_XSCREEN (window->priv->screen),
                           window->priv->xwindow,
                           x, y, width, height);
}

/**
 * vnck_window_is_on_workspace:
 * @window: a #VnckWindow.
 * @workspace: a #VnckWorkspace.
 *
 * Gets whether @window appears on @workspace.
 *
 * Return value: %TRUE if @window appears on @workspace, %FALSE otherwise.
 **/
gboolean
vnck_window_is_on_workspace (VnckWindow    *window,
                             VnckWorkspace *workspace)
{
  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (VNCK_IS_WORKSPACE (workspace), FALSE);

  return vnck_window_is_pinned (window) ||
    vnck_window_get_workspace (window) == workspace;
}

/**
 * vnck_window_is_in_viewport:
 * @window: a #VnckWindow.
 * @workspace: a #VnckWorkspace.
 *
 * Gets %TRUE if @window appears in the current viewport of @workspace.
 *
 * Return value: %TRUE if @window appears in current viewport of @workspace,
 * %FALSE otherwise.
 *
 * Since: 2.4
 **/
gboolean
vnck_window_is_in_viewport (VnckWindow    *window,
                            VnckWorkspace *workspace)
{
  CdkRectangle window_rect;
  CdkRectangle viewport_rect;

  g_return_val_if_fail (VNCK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (VNCK_IS_WORKSPACE (workspace), FALSE);

  if (vnck_window_is_pinned (window) )
    return TRUE;

  if (vnck_window_get_workspace (window) != workspace)
    return FALSE;

  viewport_rect.x = vnck_workspace_get_viewport_x (workspace);
  viewport_rect.y = vnck_workspace_get_viewport_y (workspace);
  viewport_rect.width = vnck_screen_get_width (window->priv->screen);
  viewport_rect.height = vnck_screen_get_height (window->priv->screen);

  window_rect.x = window->priv->x - window->priv->left_frame + viewport_rect.x;
  window_rect.y = window->priv->y - window->priv->top_frame + viewport_rect.y;
  window_rect.width = window->priv->width + window->priv->left_frame + window->priv->right_frame;
  window_rect.height = window->priv->height + window->priv->top_frame + window->priv->bottom_frame;

  return cdk_rectangle_intersect (&viewport_rect, &window_rect, &window_rect);
}

void
_vnck_window_set_application (VnckWindow      *window,
                              VnckApplication *app)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (app == NULL || VNCK_IS_APPLICATION (app));

  if (app)
    g_object_ref (G_OBJECT (app));
  if (window->priv->app)
    g_object_unref (G_OBJECT (window->priv->app));
  window->priv->app = app;
}

void
_vnck_window_set_class_group (VnckWindow     *window,
			      VnckClassGroup *class_group)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (class_group == NULL || VNCK_IS_CLASS_GROUP (class_group));

  if (class_group)
    g_object_ref (G_OBJECT (class_group));
  if (window->priv->class_group)
    g_object_unref (G_OBJECT (window->priv->class_group));
  window->priv->class_group = class_group;
}

void
_vnck_window_process_property_notify (VnckWindow *window,
                                      XEvent     *xevent)
{
  if (xevent->xproperty.atom ==
      _vnck_atom_get ("_NET_WM_STATE"))
    {
      window->priv->need_update_state = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
      _vnck_atom_get ("WM_STATE"))
    {
      window->priv->need_update_wm_state = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           XA_WM_NAME ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_NAME") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_VISIBLE_NAME"))
    {
      window->priv->need_update_name = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           XA_WM_ICON_NAME ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_ICON_NAME") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"))
    {
      window->priv->need_update_icon_name = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_ALLOWED_ACTIONS"))
    {
      window->priv->need_update_actions = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_DESKTOP"))
    {
      window->priv->need_update_workspace = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_WINDOW_TYPE"))
    {
      window->priv->need_update_wintype = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("WM_TRANSIENT_FOR"))
    {
      window->priv->need_update_transient_for = TRUE;
      window->priv->need_update_wintype = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_STARTUP_ID"))
    {
      window->priv->need_update_startup_id = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom == XA_WM_CLASS)
    {
      window->priv->need_update_wmclass = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_WM_ICON") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("KWM_WIN_ICON"))
    {
      _vnck_icon_cache_property_changed (window->priv->icon_cache,
                                         xevent->xproperty.atom);
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
  	   _vnck_atom_get ("WM_HINTS"))
    {
      window->priv->need_update_wmhints = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("_NET_FRAME_EXTENTS") ||
           xevent->xproperty.atom ==
           _vnck_atom_get ("_CTK_FRAME_EXTENTS"))
    {
      window->priv->need_update_frame_extents = TRUE;
      queue_update (window);
    }
  else if (xevent->xproperty.atom ==
           _vnck_atom_get ("WM_WINDOW_ROLE"))
    {
      window->priv->need_update_role = TRUE;
      queue_update (window);
    }
}

void
_vnck_window_process_configure_notify (VnckWindow *window,
                                       XEvent     *xevent)
{
  if (xevent->xconfigure.send_event)
    {
      window->priv->x = xevent->xconfigure.x;
      window->priv->y = xevent->xconfigure.y;
    }
  else
    {
      _vnck_get_window_position (VNCK_SCREEN_XSCREEN (window->priv->screen),
				 window->priv->xwindow,
                                 &window->priv->x,
                                 &window->priv->y);
    }

  window->priv->width = xevent->xconfigure.width;
  window->priv->height = xevent->xconfigure.height;

  emit_geometry_changed (window);
}

static void
update_wm_state (VnckWindow *window)
{
  int state;

  if (!window->priv->need_update_wm_state)
    return;

  window->priv->need_update_wm_state = FALSE;

  window->priv->wm_state_iconic = FALSE;

  state = _vnck_get_wm_state (VNCK_SCREEN_XSCREEN (window->priv->screen),
                              window->priv->xwindow);

  if (state == IconicState)
    window->priv->wm_state_iconic = TRUE;
}

static void
update_state (VnckWindow *window)
{
  Atom *atoms;
  int n_atoms;
  int i;
  gboolean reread_net_wm_state;

  reread_net_wm_state = window->priv->need_update_state;

  window->priv->need_update_state = FALSE;

  /* This is a bad hack, we always add the
   * state based on window type in to the state,
   * even if no state update is pending (since the
   * state update just means the _NET_WM_STATE prop
   * changed
   */

  if (reread_net_wm_state)
    {
      gboolean demanded_attention;

      demanded_attention = window->priv->demands_attention;

      window->priv->is_maximized_horz = FALSE;
      window->priv->is_maximized_vert = FALSE;
      window->priv->is_sticky = FALSE;
      window->priv->is_shaded = FALSE;
      window->priv->is_above = FALSE;
      window->priv->is_below = FALSE;
      window->priv->skip_taskbar = FALSE;
      window->priv->skip_pager = FALSE;
      window->priv->net_wm_state_hidden = FALSE;
      window->priv->is_fullscreen = FALSE;
      window->priv->demands_attention = FALSE;

      atoms = NULL;
      n_atoms = 0;
      _vnck_get_atom_list (VNCK_SCREEN_XSCREEN (window->priv->screen),
                           window->priv->xwindow,
                           _vnck_atom_get ("_NET_WM_STATE"),
                           &atoms, &n_atoms);

      i = 0;
      while (i < n_atoms)
        {
          if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_VERT"))
            window->priv->is_maximized_vert = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_MAXIMIZED_HORZ"))
            window->priv->is_maximized_horz = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_HIDDEN"))
            window->priv->net_wm_state_hidden = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_STICKY"))
            window->priv->is_sticky = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_SHADED"))
            window->priv->is_shaded = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_ABOVE"))
            window->priv->is_above = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_BELOW"))
            window->priv->is_below = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_FULLSCREEN"))
            window->priv->is_fullscreen = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_SKIP_TASKBAR"))
            window->priv->skip_taskbar = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_SKIP_PAGER"))
            window->priv->skip_pager = TRUE;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_STATE_DEMANDS_ATTENTION"))
            window->priv->demands_attention = TRUE;

          ++i;
        }

      if (window->priv->demands_attention != demanded_attention)
        {
          if (window->priv->demands_attention)
            time (&window->priv->needs_attention_time);
          else if (!window->priv->is_urgent)
            window->priv->needs_attention_time = 0;
        }

      g_free (atoms);
    }

  switch (window->priv->wintype)
    {
    case VNCK_WINDOW_DESKTOP:
    case VNCK_WINDOW_DOCK:
    case VNCK_WINDOW_SPLASHSCREEN:
      window->priv->skip_taskbar = TRUE;
      break;

    case VNCK_WINDOW_TOOLBAR:
    case VNCK_WINDOW_MENU:
    case VNCK_WINDOW_UTILITY:
    case VNCK_WINDOW_DIALOG:
      /* Skip taskbar if the window is transient
       * for some main application window
       */
      if (vnck_window_get_transient (window) != NULL &&
          !window->priv->transient_for_root)
        window->priv->skip_taskbar = TRUE;
      break;

    case VNCK_WINDOW_NORMAL:
    default:
      break;
    }

  /* FIXME!!!!!!!!!! What in the world is this buggy duplicate of the code
   * immediately above this for??!?!?
   */
  switch (window->priv->wintype)
    {
    case VNCK_WINDOW_DESKTOP:
    case VNCK_WINDOW_DOCK:
    case VNCK_WINDOW_TOOLBAR:
    case VNCK_WINDOW_MENU:
    case VNCK_WINDOW_SPLASHSCREEN:
      window->priv->skip_pager = TRUE;
      break;

    case VNCK_WINDOW_NORMAL:
    case VNCK_WINDOW_DIALOG:
    case VNCK_WINDOW_UTILITY:
    default:
      break;
    }

  /* FIXME we need to recompute this if the window manager changes */
  if (vnck_screen_net_wm_supports (window->priv->screen,
                                   "_NET_WM_STATE_HIDDEN"))
    {
      window->priv->is_hidden = window->priv->net_wm_state_hidden;

      /* FIXME this is really broken; need to bring it up on
       * wm-spec-list. It results in showing an "Unminimize" menu
       * item on task list, for shaded windows.
       */
      window->priv->is_minimized = window->priv->is_hidden;
    }
  else
    {
      window->priv->is_minimized = window->priv->wm_state_iconic;

      window->priv->is_hidden = window->priv->is_minimized || window->priv->is_shaded;
    }
}

static void
update_name (VnckWindow *window)
{
  char *new_name;

  if (!window->priv->need_update_name)
    return;

  window->priv->need_update_name = FALSE;

  new_name = _vnck_get_name (VNCK_SCREEN_XSCREEN (window->priv->screen),
                             window->priv->xwindow);

  if (g_strcmp0 (window->priv->name, new_name) != 0)
    window->priv->need_emit_name_changed = TRUE;

  g_free (window->priv->name);
  window->priv->name = new_name;
}

static void
update_icon_name (VnckWindow *window)
{
  char *new_name = NULL;

  if (!window->priv->need_update_icon_name)
    return;

  window->priv->need_update_icon_name = FALSE;

  new_name = _vnck_get_icon_name (VNCK_SCREEN_XSCREEN (window->priv->screen),
                                  window->priv->xwindow);

  if (g_strcmp0 (window->priv->icon_name, new_name) != 0)
    window->priv->need_emit_name_changed = TRUE;

  g_free (window->priv->icon_name);
  window->priv->icon_name = new_name;
}

static void
update_workspace (VnckWindow *window)
{
  int val;
  int old;

  if (!window->priv->need_update_workspace)
    return;

  window->priv->need_update_workspace = FALSE;

  old = window->priv->workspace;

  val = ALL_WORKSPACES;
  _vnck_get_cardinal (VNCK_SCREEN_XSCREEN (window->priv->screen),
                      window->priv->xwindow,
                      _vnck_atom_get ("_NET_WM_DESKTOP"),
                      &val);

  window->priv->workspace = val;

  if (old != window->priv->workspace)
    emit_workspace_changed (window);
}

static void
update_actions (VnckWindow *window)
{
  Atom *atoms;
  int   n_atoms;
  int   i;

  if (!window->priv->need_update_actions)
    return;

  window->priv->need_update_actions = FALSE;

  window->priv->actions = 0;

  atoms = NULL;
  n_atoms = 0;
  if (!_vnck_get_atom_list (VNCK_SCREEN_XSCREEN (window->priv->screen),
                            window->priv->xwindow,
                            _vnck_atom_get ("_NET_WM_ALLOWED_ACTIONS"),
                            &atoms,
                            &n_atoms))
    {
      window->priv->actions =
                VNCK_WINDOW_ACTION_MOVE                    |
                VNCK_WINDOW_ACTION_RESIZE                  |
                VNCK_WINDOW_ACTION_SHADE                   |
                VNCK_WINDOW_ACTION_STICK                   |
                VNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   |
                VNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     |
                VNCK_WINDOW_ACTION_CHANGE_WORKSPACE        |
                VNCK_WINDOW_ACTION_CLOSE                   |
                VNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY |
                VNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY   |
                VNCK_WINDOW_ACTION_UNSHADE                 |
                VNCK_WINDOW_ACTION_UNSTICK                 |
                VNCK_WINDOW_ACTION_MINIMIZE                |
                VNCK_WINDOW_ACTION_UNMINIMIZE              |
                VNCK_WINDOW_ACTION_MAXIMIZE                |
                VNCK_WINDOW_ACTION_UNMAXIMIZE              |
                VNCK_WINDOW_ACTION_FULLSCREEN              |
                VNCK_WINDOW_ACTION_ABOVE                   |
                VNCK_WINDOW_ACTION_BELOW;
      return;
    }

  i = 0;
  while (i < n_atoms)
    {
      if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_MOVE"))
        window->priv->actions |= VNCK_WINDOW_ACTION_MOVE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_RESIZE"))
        window->priv->actions |= VNCK_WINDOW_ACTION_RESIZE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_SHADE"))
        window->priv->actions |= VNCK_WINDOW_ACTION_SHADE |
                                 VNCK_WINDOW_ACTION_UNSHADE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_STICK"))
        window->priv->actions |= VNCK_WINDOW_ACTION_STICK |
                                 VNCK_WINDOW_ACTION_UNSTICK;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_MINIMIZE"))
	window->priv->actions |= VNCK_WINDOW_ACTION_MINIMIZE   |
	                         VNCK_WINDOW_ACTION_UNMINIMIZE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_MAXIMIZE_HORZ"))
        window->priv->actions |= VNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY |
                                 VNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_MAXIMIZE_VERT"))
        window->priv->actions |= VNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY |
                                 VNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_CHANGE_DESKTOP"))
        window->priv->actions |= VNCK_WINDOW_ACTION_CHANGE_WORKSPACE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_CLOSE"))
        window->priv->actions |= VNCK_WINDOW_ACTION_CLOSE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_FULLSCREEN"))
        window->priv->actions |= VNCK_WINDOW_ACTION_FULLSCREEN;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_ABOVE"))
        window->priv->actions |= VNCK_WINDOW_ACTION_ABOVE;

      else if (atoms[i] == _vnck_atom_get ("_NET_WM_ACTION_BELOW"))
        window->priv->actions |= VNCK_WINDOW_ACTION_BELOW;

      else
        {
          const char *name = _vnck_atom_name (atoms [i]);

          if (name && g_str_has_prefix (name, "_NET_WM_"))
            g_warning ("Unhandled action type %s", name);
        }

      i++;
    }

  g_free (atoms);

  if ((window->priv->actions & VNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY) &&
      (window->priv->actions & VNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY))
    window->priv->actions |=
        VNCK_WINDOW_ACTION_MAXIMIZE   |
        VNCK_WINDOW_ACTION_UNMAXIMIZE;
}

static void
update_wintype (VnckWindow *window)
{
  Atom *atoms;
  int n_atoms;
  VnckWindowType type;
  gboolean found_type;

  if (!window->priv->need_update_wintype)
    return;

  window->priv->need_update_wintype = FALSE;

  found_type = FALSE;
  type = VNCK_WINDOW_NORMAL;

  atoms = NULL;
  n_atoms = 0;
  if (_vnck_get_atom_list (VNCK_SCREEN_XSCREEN (window->priv->screen),
                           window->priv->xwindow,
                           _vnck_atom_get ("_NET_WM_WINDOW_TYPE"),
                           &atoms,
                           &n_atoms))
    {
      int i;

      i = 0;
      while (i < n_atoms && !found_type)
        {
          /* We break as soon as we find one we recognize,
           * supposed to prefer those near the front of the list
           */
          found_type = TRUE;
          if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DESKTOP"))
            type = VNCK_WINDOW_DESKTOP;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DOCK"))
            type = VNCK_WINDOW_DOCK;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_TOOLBAR"))
            type = VNCK_WINDOW_TOOLBAR;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_MENU"))
            type = VNCK_WINDOW_MENU;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_DIALOG"))
            type = VNCK_WINDOW_DIALOG;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_NORMAL"))
            type = VNCK_WINDOW_NORMAL;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_UTILITY"))
            type = VNCK_WINDOW_UTILITY;
          else if (atoms[i] == _vnck_atom_get ("_NET_WM_WINDOW_TYPE_SPLASH"))
            type = VNCK_WINDOW_SPLASHSCREEN;
          else
            found_type = FALSE;

          ++i;
        }

      g_free (atoms);
    }

  if (!found_type)
    {
      if (window->priv->transient_for != None)
        {
          type = VNCK_WINDOW_DIALOG;
        }
      else
        {
          type = VNCK_WINDOW_NORMAL;
        }
      found_type = TRUE;
    }

  if (window->priv->wintype != type)
    {
      window->priv->need_emit_type_changed = TRUE;
      window->priv->wintype = type;
    }
}

static void
update_transient_for (VnckWindow *window)
{
  Window parent;

  if (!window->priv->need_update_transient_for)
    return;

  window->priv->need_update_transient_for = FALSE;

  parent = None;
  if (_vnck_get_window (VNCK_SCREEN_XSCREEN (window->priv->screen),
                        window->priv->xwindow,
                        _vnck_atom_get ("WM_TRANSIENT_FOR"),
                        &parent) &&
      parent != window->priv->xwindow)
    {
      window->priv->transient_for = parent;

      if (vnck_screen_get_for_root (window->priv->transient_for) != NULL)
        window->priv->transient_for_root = TRUE;
      else
        window->priv->transient_for_root = FALSE;
    }
  else
    {
      window->priv->transient_for = None;
      window->priv->transient_for_root = FALSE;
    }
}

static void
update_startup_id (VnckWindow *window)
{
  if (!window->priv->need_update_startup_id)
    return;

  window->priv->need_update_startup_id = FALSE;

  g_free (window->priv->startup_id);
  window->priv->startup_id =
    _vnck_get_utf8_property (VNCK_SCREEN_XSCREEN (window->priv->screen),
                             window->priv->xwindow,
                             _vnck_atom_get ("_NET_STARTUP_ID"));
}

static void
update_wmclass (VnckWindow *window)
{
  char *new_res_class = NULL;
  char *new_res_name = NULL;

  if (!window->priv->need_update_wmclass)
    return;

  window->priv->need_update_wmclass = FALSE;

  _vnck_get_wmclass (VNCK_SCREEN_XSCREEN (window->priv->screen),
                     window->priv->xwindow,
                     &new_res_class,
                     &new_res_name);

  if (g_strcmp0 (window->priv->res_class, new_res_class) != 0 ||
      g_strcmp0 (window->priv->res_name, new_res_name) != 0)
    {
      window->priv->need_emit_class_changed = TRUE;

      g_free (window->priv->res_class);
      g_free (window->priv->res_name);

      window->priv->res_class = new_res_class;
      window->priv->res_name = new_res_name;
    }
  else
    {
      g_free (new_res_class);
      g_free (new_res_name);
    }
}

static void
update_wmhints (VnckWindow *window)
{
  Display  *display;
  XWMHints *hints;

  if (!window->priv->need_update_wmhints)
    return;

  display = _vnck_window_get_display (window);

  _vnck_error_trap_push (display);
  hints = XGetWMHints (display, window->priv->xwindow);
  _vnck_error_trap_pop (display);

  if (hints)
    {
      if ((hints->flags & IconPixmapHint) ||
          (hints->flags & IconMaskHint))
        _vnck_icon_cache_property_changed (window->priv->icon_cache,
                                           _vnck_atom_get ("WM_HINTS"));

      if (hints->flags & WindowGroupHint)
          window->priv->group_leader = hints->window_group;

      if (hints->flags & XUrgencyHint)
        {
          window->priv->is_urgent = TRUE;
          time (&window->priv->needs_attention_time);
        }
      else
        {
          window->priv->is_urgent = FALSE;
          if (!window->priv->demands_attention)
            window->priv->needs_attention_time = 0;
        }

      XFree (hints);
    }

  window->priv->need_update_wmhints = FALSE;
}

static void
update_frame_extents (VnckWindow *window)
{
  int left, right, top, bottom;

  if (!window->priv->need_update_frame_extents)
    return;

  window->priv->need_update_frame_extents = FALSE;

  left = right = top = bottom = 0;

  if (!_vnck_get_frame_extents (VNCK_SCREEN_XSCREEN (window->priv->screen),
                                window->priv->xwindow,
                                &left, &right, &top, &bottom))
    return;

  if (left   != window->priv->left_frame ||
      right  != window->priv->right_frame ||
      top    != window->priv->top_frame ||
      bottom != window->priv->bottom_frame)
    {
      window->priv->left_frame   = left;
      window->priv->right_frame  = right;
      window->priv->top_frame    = top;
      window->priv->bottom_frame = bottom;

      emit_geometry_changed (window);
    }
}

static void
update_role (VnckWindow *window)
{
  char *new_role;

  if (!window->priv->need_update_role)
    return;

  window->priv->need_update_role = FALSE;

  new_role = _vnck_get_text_property (VNCK_SCREEN_XSCREEN (window->priv->screen),
                                      window->priv->xwindow,
                                      _vnck_atom_get ("WM_WINDOW_ROLE"));

  if (g_strcmp0 (window->priv->role, new_role) != 0)
    {
      window->priv->need_emit_role_changed = TRUE;

      g_free (window->priv->role);
      window->priv->role = new_role;
    }
  else
    {
      g_free (new_role);
    }
}

static void
force_update_now (VnckWindow *window)
{
  VnckWindowState old_state;
  VnckWindowState new_state;
  VnckWindowActions old_actions;

  unqueue_update (window);

  /* Name must be done before all other stuff,
   * because we have iconsistent state across the
   * update_name/update_icon_name functions (no window name),
   * and we have to fix that before we emit any other signals
   */

  update_name (window);
  update_icon_name (window);

  if (window->priv->need_emit_name_changed)
    emit_name_changed (window);

  old_state = COMPRESS_STATE (window);
  old_actions = window->priv->actions;

  update_startup_id (window);    /* no side effects */
  update_wmclass (window);
  update_wmhints (window);
  update_transient_for (window); /* wintype needs this to be first */
  update_wintype (window);   /* emits signals */
  update_wm_state (window);
  update_state (window);     /* must come after the above, since they affect
                              * our calculated state
                              */
  update_workspace (window); /* emits signals */
  update_actions (window);
  update_frame_extents (window); /* emits signals */
  update_role (window); /* emits signals */

  get_icons (window);

  new_state = COMPRESS_STATE (window);

  if (old_state != new_state)
    emit_state_changed (window, old_state ^ new_state, new_state);

  if (old_actions != window->priv->actions)
    emit_actions_changed (window, old_actions ^ window->priv->actions,
                          window->priv->actions);

  if (window->priv->need_emit_icon_changed)
    emit_icon_changed (window);

  if (window->priv->need_emit_class_changed)
    emit_class_changed (window);

  if (window->priv->need_emit_role_changed)
    emit_role_changed (window);

  if (window->priv->need_emit_type_changed)
    emit_type_changed (window);
}


static gboolean
update_idle (gpointer data)
{
  VnckWindow *window = VNCK_WINDOW (data);

  window->priv->update_handler = 0;
  force_update_now (window);
  return FALSE;
}

static void
queue_update (VnckWindow *window)
{
  if (window->priv->update_handler != 0)
    return;

  window->priv->update_handler = g_idle_add (update_idle, window);
}

static void
unqueue_update (VnckWindow *window)
{
  if (window->priv->update_handler != 0)
    {
      g_source_remove (window->priv->update_handler);
      window->priv->update_handler = 0;
    }
}

static void
emit_name_changed (VnckWindow *window)
{
  window->priv->need_emit_name_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[NAME_CHANGED],
                 0);
}

static void
emit_state_changed (VnckWindow     *window,
                    VnckWindowState changed_mask,
                    VnckWindowState new_state)
{
  g_signal_emit (G_OBJECT (window),
                 signals[STATE_CHANGED],
                 0, changed_mask, new_state);
}

static void
emit_workspace_changed (VnckWindow *window)
{
  g_signal_emit (G_OBJECT (window),
                 signals[WORKSPACE_CHANGED],
                 0);
}

static void
emit_icon_changed (VnckWindow *window)
{
  window->priv->need_emit_icon_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[ICON_CHANGED],
                 0);
}

static void
emit_class_changed (VnckWindow *window)
{
  window->priv->need_emit_class_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[CLASS_CHANGED],
                 0);
}

static void
emit_actions_changed   (VnckWindow       *window,
                        VnckWindowActions changed_mask,
                        VnckWindowActions new_actions)
{
  g_signal_emit (G_OBJECT (window),
                 signals[ACTIONS_CHANGED],
                 0, changed_mask, new_actions);
}

static void
emit_geometry_changed (VnckWindow *window)
{
  g_signal_emit (G_OBJECT (window),
                 signals[GEOMETRY_CHANGED],
                 0);
}

static void
emit_role_changed (VnckWindow *window)
{
  window->priv->need_emit_role_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[ROLE_CHANGED],
                 0);
}

static void
emit_type_changed (VnckWindow *window)
{
  window->priv->need_emit_type_changed = FALSE;
  g_signal_emit (G_OBJECT (window),
                 signals[TYPE_CHANGED],
                 0);
}
