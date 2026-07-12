/* window object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef VNCK_WINDOW_H
#define VNCK_WINDOW_H

#ifndef VNCK_I_KNOW_THIS_IS_UNSTABLE
#error "libvnck should only be used if you understand that it's subject to frequent change, and is not supported as a fixed API/ABI or as part of the platform"
#endif

#include <glib-object.h>
#include <libvnck/screen.h>
#include <cdk-pixbuf/cdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * VnckWindowState:
 * @VNCK_WINDOW_STATE_MINIMIZED: the window is minimized.
 * @VNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY: the window is horizontically
 * maximized.
 * @VNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY: the window is vertically maximized.
 * @VNCK_WINDOW_STATE_SHADED: the window is shaded.
 * @VNCK_WINDOW_STATE_SKIP_PAGER: the window should not be included on pagers.
 * @VNCK_WINDOW_STATE_SKIP_TASKLIST: the window should not be included on
 * tasklists.
 * @VNCK_WINDOW_STATE_STICKY: the window is sticky (see
 * vnck_window_is_sticky()).
 * @VNCK_WINDOW_STATE_HIDDEN: the window is not visible on its #VnckWorkspace
 * and viewport (when minimized, for example).
 * @VNCK_WINDOW_STATE_FULLSCREEN: the window is fullscreen.
 * @VNCK_WINDOW_STATE_DEMANDS_ATTENTION: the window needs attention (because
 * the window requested activation but the window manager refused it, for
 * example).
 * @VNCK_WINDOW_STATE_URGENT: the window requires a response from the user.
 * @VNCK_WINDOW_STATE_ABOVE: the window is above other windows (see
 * vnck_window_make_above()).
 * @VNCK_WINDOW_STATE_BELOW: the window is below other windows (see
 * vnck_window_make_below()).
 *
 * Type used as a bitmask to describe the state of a #VnckWindow.
 */
typedef enum
{
  VNCK_WINDOW_STATE_MINIMIZED              = 1 << 0,
  VNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY = 1 << 1,
  VNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY   = 1 << 2,
  VNCK_WINDOW_STATE_SHADED                 = 1 << 3,
  VNCK_WINDOW_STATE_SKIP_PAGER             = 1 << 4,
  VNCK_WINDOW_STATE_SKIP_TASKLIST          = 1 << 5,
  VNCK_WINDOW_STATE_STICKY                 = 1 << 6,
  VNCK_WINDOW_STATE_HIDDEN                 = 1 << 7,
  VNCK_WINDOW_STATE_FULLSCREEN             = 1 << 8,
  VNCK_WINDOW_STATE_DEMANDS_ATTENTION      = 1 << 9,
  VNCK_WINDOW_STATE_URGENT                 = 1 << 10,
  VNCK_WINDOW_STATE_ABOVE                  = 1 << 11,
  VNCK_WINDOW_STATE_BELOW                  = 1 << 12
} VnckWindowState;

/**
 * VnckWindowActions:
 * @VNCK_WINDOW_ACTION_MOVE: the window may be moved around the screen. 
 * @VNCK_WINDOW_ACTION_RESIZE: the window may be resized.
 * @VNCK_WINDOW_ACTION_SHADE: the window may be shaded.
 * @VNCK_WINDOW_ACTION_STICK: the window may be sticked.
 * @VNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY: the window may be maximized
 * horizontally.
 * @VNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY: the window may be maximized
 * vertically.
 * @VNCK_WINDOW_ACTION_CHANGE_WORKSPACE: the window may be moved between
 * workspaces, or (un)pinned.
 * @VNCK_WINDOW_ACTION_CLOSE: the window may be closed.
 * @VNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY: the window may be unmaximized
 * horizontally.
 * @VNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY: the window may be maximized
 * vertically.
 * @VNCK_WINDOW_ACTION_UNSHADE: the window may be unshaded.
 * @VNCK_WINDOW_ACTION_UNSTICK: the window may be unsticked.
 * @VNCK_WINDOW_ACTION_MINIMIZE: the window may be minimized.
 * @VNCK_WINDOW_ACTION_UNMINIMIZE: the window may be unminimized.
 * @VNCK_WINDOW_ACTION_MAXIMIZE: the window may be maximized.
 * @VNCK_WINDOW_ACTION_UNMAXIMIZE: the window may be unmaximized.
 * @VNCK_WINDOW_ACTION_FULLSCREEN: the window may be brought to fullscreen.
 * @VNCK_WINDOW_ACTION_ABOVE: the window may be made above other windows.
 * @VNCK_WINDOW_ACTION_BELOW: the window may be made below other windows.
 *
 * Type used as a bitmask to describe the actions that can be done for a
 * #VnckWindow.
 */
typedef enum
{
  VNCK_WINDOW_ACTION_MOVE                    = 1 << 0,
  VNCK_WINDOW_ACTION_RESIZE                  = 1 << 1,
  VNCK_WINDOW_ACTION_SHADE                   = 1 << 2,
  VNCK_WINDOW_ACTION_STICK                   = 1 << 3,
  VNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   = 1 << 4,
  VNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     = 1 << 5,
  VNCK_WINDOW_ACTION_CHANGE_WORKSPACE        = 1 << 6, /* includes pin/unpin */
  VNCK_WINDOW_ACTION_CLOSE                   = 1 << 7,
  VNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY = 1 << 8,
  VNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY   = 1 << 9,
  VNCK_WINDOW_ACTION_UNSHADE                 = 1 << 10,
  VNCK_WINDOW_ACTION_UNSTICK                 = 1 << 11,
  VNCK_WINDOW_ACTION_MINIMIZE                = 1 << 12,
  VNCK_WINDOW_ACTION_UNMINIMIZE              = 1 << 13,
  VNCK_WINDOW_ACTION_MAXIMIZE                = 1 << 14,
  VNCK_WINDOW_ACTION_UNMAXIMIZE              = 1 << 15,
  VNCK_WINDOW_ACTION_FULLSCREEN              = 1 << 16,
  VNCK_WINDOW_ACTION_ABOVE                   = 1 << 17,
  VNCK_WINDOW_ACTION_BELOW                   = 1 << 18
} VnckWindowActions;

/**
 * VnckWindowType:
 * @VNCK_WINDOW_NORMAL: the window is a normal window.
 * @VNCK_WINDOW_DESKTOP: the window is a desktop.
 * @VNCK_WINDOW_DOCK: the window is a dock or a panel.
 * @VNCK_WINDOW_DIALOG: the window is a dialog window.
 * @VNCK_WINDOW_TOOLBAR: the window is a tearoff toolbar.
 * @VNCK_WINDOW_MENU: the window is a tearoff menu.
 * @VNCK_WINDOW_UTILITY: the window is a small persistent utility window, such
 * as a palette or toolbox.
 * @VNCK_WINDOW_SPLASHSCREEN: the window is a splash screen displayed as an
 * application is starting up.
 *
 * Type describing the semantic type of a #VnckWindow.
 */
typedef enum
{
  VNCK_WINDOW_NORMAL,       /* document/app window */
  VNCK_WINDOW_DESKTOP,      /* desktop background */
  VNCK_WINDOW_DOCK,         /* panel */
  VNCK_WINDOW_DIALOG,       /* dialog */
  VNCK_WINDOW_TOOLBAR,      /* tearoff toolbar */
  VNCK_WINDOW_MENU,         /* tearoff menu */
  VNCK_WINDOW_UTILITY,      /* palette/toolbox window */
  VNCK_WINDOW_SPLASHSCREEN  /* splash screen */
} VnckWindowType;

/**
 * VnckWindowGravity:
 * @VNCK_WINDOW_GRAVITY_CURRENT: keep the current gravity point.
 * @VNCK_WINDOW_GRAVITY_NORTHWEST: use the left top corner of the frame window
 * as gravity point.
 * @VNCK_WINDOW_GRAVITY_NORTH: use the center of the frame window's top side as
 * gravity point.
 * @VNCK_WINDOW_GRAVITY_NORTHEAST: use the right top corner of the frame window
 * as gravity point.
 * @VNCK_WINDOW_GRAVITY_WEST: use the center of the frame window's left side as
 * gravity point.
 * @VNCK_WINDOW_GRAVITY_CENTER: use the center of the frame window as gravity
 * point.
 * @VNCK_WINDOW_GRAVITY_EAST: use the center of the frame window's right side
 * as gravity point.
 * @VNCK_WINDOW_GRAVITY_SOUTHWEST: use the left bottom corner of the frame
 * window as gravity point.
 * @VNCK_WINDOW_GRAVITY_SOUTH: use the center of the frame window's bottom side
 * as gravity point.
 * @VNCK_WINDOW_GRAVITY_SOUTHEAST: use the right bottom corner of the frame
 * window as gravity point.
 * @VNCK_WINDOW_GRAVITY_STATIC: use the left top corner of the client window as
 * gravity point.
 *
 * Flag used when changing the geometry of a #VnckWindow. This is the gravity
 * point to use as a reference for the new position.
 *
 * Since: 2.16
 */
typedef enum
{
  VNCK_WINDOW_GRAVITY_CURRENT   = 0,
  VNCK_WINDOW_GRAVITY_NORTHWEST = 1,
  VNCK_WINDOW_GRAVITY_NORTH     = 2,
  VNCK_WINDOW_GRAVITY_NORTHEAST = 3,
  VNCK_WINDOW_GRAVITY_WEST      = 4,
  VNCK_WINDOW_GRAVITY_CENTER    = 5,
  VNCK_WINDOW_GRAVITY_EAST      = 6,
  VNCK_WINDOW_GRAVITY_SOUTHWEST = 7,
  VNCK_WINDOW_GRAVITY_SOUTH     = 8,
  VNCK_WINDOW_GRAVITY_SOUTHEAST = 9,
  VNCK_WINDOW_GRAVITY_STATIC    = 10
} VnckWindowGravity;

/**
 * VnckWindowMoveResizeMask:
 * @VNCK_WINDOW_CHANGE_X: X coordinate of the window should be changed.
 * @VNCK_WINDOW_CHANGE_Y: Y coordinate of the window should be changed.
 * @VNCK_WINDOW_CHANGE_WIDTH: width of the window should be changed.
 * @VNCK_WINDOW_CHANGE_HEIGHT: height of the window should be changed.
 *
 * Flag used as a bitmask when changing the geometry of a #VnckWindow. This
 * indicates which part of the geometry should be changed.
 *
 * Since: 2.16
 */
typedef enum
{
  VNCK_WINDOW_CHANGE_X      = 1 << 0,
  VNCK_WINDOW_CHANGE_Y      = 1 << 1,
  VNCK_WINDOW_CHANGE_WIDTH  = 1 << 2,
  VNCK_WINDOW_CHANGE_HEIGHT = 1 << 3
} VnckWindowMoveResizeMask;

#define VNCK_TYPE_WINDOW              (vnck_window_get_type ())
#define VNCK_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_WINDOW, VnckWindow))
#define VNCK_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_WINDOW, VnckWindowClass))
#define VNCK_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_WINDOW))
#define VNCK_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_WINDOW))
#define VNCK_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_WINDOW, VnckWindowClass))

typedef struct _VnckWindowClass   VnckWindowClass;
typedef struct _VnckWindowPrivate VnckWindowPrivate;

/**
 * VnckWindow:
 *
 * The #VnckWindow struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckWindow
{
  GObject parent_instance;

  VnckWindowPrivate *priv;
};

struct _VnckWindowClass
{
  GObjectClass parent_class;

  /* window name or icon name changed */
  void (* name_changed) (VnckWindow *window);

  /* minimized, maximized, sticky, skip pager, skip task, shaded
   * may have changed
   */
  void (* state_changed) (VnckWindow     *window,
                          VnckWindowState changed_mask,
                          VnckWindowState new_state);

  /* Changed workspace or pinned/unpinned state */
  void (* workspace_changed) (VnckWindow *window);

  /* Changed icon */
  void (* icon_changed)      (VnckWindow *window);

  /* Changed actions */
  void (* actions_changed)   (VnckWindow       *window,
                              VnckWindowActions changed_mask,
                              VnckWindowActions new_actions);

  /* Changed size/position */
  void (* geometry_changed)  (VnckWindow       *window);

  /* Changed class group/instance name */
  void (* class_changed)     (VnckWindow       *window);

  /* Changed role */
  void (* role_changed)      (VnckWindow       *window);

  void (* type_changed)      (VnckWindow       *window);

  /* Padding for future expansion */
  void (* pad1) (void);
};

GType vnck_window_get_type (void) G_GNUC_CONST;

VnckWindow* vnck_window_get (gulong xwindow);

VnckScreen* vnck_window_get_screen    (VnckWindow *window);

gboolean    vnck_window_has_name      (VnckWindow *window);
const char* vnck_window_get_name      (VnckWindow *window);
gboolean    vnck_window_has_icon_name (VnckWindow *window);
const char* vnck_window_get_icon_name (VnckWindow *window);

VnckApplication* vnck_window_get_application  (VnckWindow *window);
VnckWindow*      vnck_window_get_transient    (VnckWindow *window);
gulong           vnck_window_get_group_leader (VnckWindow *window);
gulong           vnck_window_get_xid          (VnckWindow *window);

VnckClassGroup *vnck_window_get_class_group   (VnckWindow *window);

const char* vnck_window_get_class_group_name    (VnckWindow *window);
const char* vnck_window_get_class_instance_name (VnckWindow *window);

const char* vnck_window_get_session_id        (VnckWindow *window);
const char* vnck_window_get_session_id_utf8   (VnckWindow *window);
const char* vnck_window_get_role              (VnckWindow *window);
int         vnck_window_get_pid               (VnckWindow *window);
gint        vnck_window_get_sort_order        (VnckWindow *window);
void        vnck_window_set_sort_order        (VnckWindow *window,
						gint order);

VnckWindowType vnck_window_get_window_type    (VnckWindow *window);
void           vnck_window_set_window_type    (VnckWindow *window,
                                               VnckWindowType wintype);

gboolean vnck_window_is_minimized              (VnckWindow *window);
gboolean vnck_window_is_maximized_horizontally (VnckWindow *window);
gboolean vnck_window_is_maximized_vertically   (VnckWindow *window);
gboolean vnck_window_is_maximized              (VnckWindow *window);
gboolean vnck_window_is_shaded                 (VnckWindow *window);
gboolean vnck_window_is_above                  (VnckWindow *window);
gboolean vnck_window_is_below                  (VnckWindow *window);
gboolean vnck_window_is_skip_pager             (VnckWindow *window);
gboolean vnck_window_is_skip_tasklist          (VnckWindow *window);
gboolean vnck_window_is_fullscreen             (VnckWindow *window);
gboolean vnck_window_is_sticky                 (VnckWindow *window);
gboolean vnck_window_needs_attention           (VnckWindow *window);
gboolean vnck_window_or_transient_needs_attention (VnckWindow *window);

void vnck_window_set_skip_pager    (VnckWindow *window,
                                    gboolean skip);
void vnck_window_set_skip_tasklist (VnckWindow *window,
                                    gboolean skip);
void vnck_window_set_fullscreen (VnckWindow *window,
                                 gboolean fullscreen);

void vnck_window_close                   (VnckWindow *window,
                                          guint32     timestamp);
void vnck_window_minimize                (VnckWindow *window);
void vnck_window_unminimize              (VnckWindow *window,
                                          guint32     timestamp);
void vnck_window_maximize                (VnckWindow *window);
void vnck_window_unmaximize              (VnckWindow *window);
void vnck_window_maximize_horizontally   (VnckWindow *window);
void vnck_window_unmaximize_horizontally (VnckWindow *window);
void vnck_window_maximize_vertically     (VnckWindow *window);
void vnck_window_unmaximize_vertically   (VnckWindow *window);
void vnck_window_shade                   (VnckWindow *window);
void vnck_window_unshade                 (VnckWindow *window);
void vnck_window_make_above              (VnckWindow *window);
void vnck_window_unmake_above            (VnckWindow *window);
void vnck_window_make_below              (VnckWindow *window);
void vnck_window_unmake_below            (VnckWindow *window);
void vnck_window_stick                   (VnckWindow *window);
void vnck_window_unstick                 (VnckWindow *window);
void vnck_window_keyboard_move           (VnckWindow *window);
void vnck_window_keyboard_size           (VnckWindow *window);

VnckWorkspace* vnck_window_get_workspace     (VnckWindow    *window);
void           vnck_window_move_to_workspace (VnckWindow    *window,
                                              VnckWorkspace *space);

/* pinned = on all workspaces */
gboolean vnck_window_is_pinned (VnckWindow *window);
void     vnck_window_pin       (VnckWindow *window);
void     vnck_window_unpin     (VnckWindow *window);

void     vnck_window_activate  (VnckWindow *window,
                                guint32     timestamp);
gboolean vnck_window_is_active (VnckWindow *window);
gboolean vnck_window_is_most_recently_activated (VnckWindow *window);
void     vnck_window_activate_transient (VnckWindow *window,
                                         guint32     timestamp);
gboolean vnck_window_transient_is_most_recently_activated (VnckWindow *window);

GdkPixbuf* vnck_window_get_icon      (VnckWindow *window);
GdkPixbuf* vnck_window_get_mini_icon (VnckWindow *window);

gboolean vnck_window_get_icon_is_fallback (VnckWindow *window);

void vnck_window_set_icon_geometry        (VnckWindow *window,
					   int         x,
					   int         y,
					   int         width,
					   int         height);

VnckWindowActions vnck_window_get_actions (VnckWindow *window);
VnckWindowState   vnck_window_get_state   (VnckWindow *window);

void vnck_window_get_client_window_geometry (VnckWindow *window,
                                             int        *xp,
                                             int        *yp,
                                             int        *widthp,
                                             int        *heightp);
void vnck_window_get_geometry (VnckWindow *window,
                               int        *xp,
                               int        *yp,
                               int        *widthp,
                               int        *heightp);
void vnck_window_set_geometry (VnckWindow               *window,
                               VnckWindowGravity         gravity,
                               VnckWindowMoveResizeMask  geometry_mask,
                               int                       x,
                               int                       y,
                               int                       width,
                               int                       height);

gboolean vnck_window_is_visible_on_workspace (VnckWindow    *window,
                                              VnckWorkspace *workspace);
gboolean vnck_window_is_on_workspace         (VnckWindow    *window,
                                              VnckWorkspace *workspace);
gboolean vnck_window_is_in_viewport          (VnckWindow    *window,
                                              VnckWorkspace *workspace);

G_END_DECLS

#endif /* VNCK_WINDOW_H */
