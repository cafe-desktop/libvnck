/* screen object */
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

#ifndef VNCK_SCREEN_H
#define VNCK_SCREEN_H

#include <glib-object.h>

G_BEGIN_DECLS

/* forward decls */
typedef struct _VnckApplication VnckApplication;
typedef struct _VnckClassGroup  VnckClassGroup;
typedef struct _VnckWindow      VnckWindow;
typedef struct _VnckWorkspace   VnckWorkspace;

/* Screen */

#define VNCK_TYPE_SCREEN              (vnck_screen_get_type ())
#define VNCK_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_SCREEN, VnckScreen))
#define VNCK_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_SCREEN, VnckScreenClass))
#define VNCK_IS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_SCREEN))
#define VNCK_IS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_SCREEN))
#define VNCK_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_SCREEN, VnckScreenClass))

typedef struct _VnckScreen        VnckScreen;
typedef struct _VnckScreenClass   VnckScreenClass;
typedef struct _VnckScreenPrivate VnckScreenPrivate;

/**
 * VnckScreen:
 *
 * The #VnckScreen struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckScreen
{
  GObject parent_instance;

  VnckScreenPrivate *priv;
};

struct _VnckScreenClass
{
  GObjectClass parent_class;

  /* focused window changed */
  void (* active_window_changed)    (VnckScreen *screen,
                                     VnckWindow *previous_window);
  /* current workspace changed */
  void (* active_workspace_changed) (VnckScreen *screen,
                                     VnckWorkspace *previous_workspace);
  /* stacking order changed */
  void (* window_stacking_changed)  (VnckScreen *screen);
  /* window added */
  void (* window_opened)            (VnckScreen *screen,
                                     VnckWindow *window);
  /* window removed */
  void (* window_closed)            (VnckScreen *screen,
                                     VnckWindow *window);
  /* new workspace */
  void (* workspace_created)        (VnckScreen *screen,
                                     VnckWorkspace *space);
  /* workspace gone */
  void (* workspace_destroyed)      (VnckScreen *screen,
                                     VnckWorkspace *space);
  /* new app */
  void (* application_opened)       (VnckScreen      *screen,
                                     VnckApplication *app);
  /* app gone */
  void (* application_closed)       (VnckScreen      *screen,
                                     VnckApplication *app);

  /* New background */
  void (* background_changed)       (VnckScreen      *screen);

  /* new class group */
  void (* class_group_opened)       (VnckScreen     *screen,
                                     VnckClassGroup *class_group);
  /* class group gone */
  void (* class_group_closed)       (VnckScreen     *screen,
                                     VnckClassGroup *class_group);
  /* Toggle showing desktop */
  void (* showing_desktop_changed)  (VnckScreen      *screen);

  /* Viewport stuff changed */
  void (* viewports_changed)        (VnckScreen      *screen);
  
  /* Window manager changed */
  void (* window_manager_changed)   (VnckScreen      *screen);

  /* Padding for future expansion */
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
  void (* pad5) (void);
  void (* pad6) (void);
};

#ifndef VNCK_DISABLE_DEPRECATED
typedef struct _VnckWorkspaceLayout VnckWorkspaceLayout;

/**
 * VnckWorkspaceLayout:
 * @rows: number of rows in the layout grid.
 * @cols: number of columns in the layout grid.
 * @grid: array of size @grid_area containing the index (starting from 0) of
 * the #VnckWorkspace for each position in the layout grid, or -1 if the
 * position does not correspond to any #VnckWorkspace.
 * @grid_area: size of the grid containing all #VnckWorkspace. This can be
 * bigger than the number of #VnckWorkspace because the grid might not be
 * filled.
 * @current_row: row of the specific #VnckWorkspace, starting from 0.
 * @current_col: column of the specific #VnckWorkspace, starting from 0.
 *
 * The #VnckWorkspaceLayout struct contains information about the layout of
 * #VnckWorkspace on a #VnckScreen, and the exact position of a specific
 * #VnckWorkspace.
 *
 * Since: 2.12
 * Deprecated:2.20:
 */
struct _VnckWorkspaceLayout
{
  int rows;
  int cols;
  int *grid;
  int grid_area;
  int current_row;
  int current_col;
};
#endif /* VNCK_DISABLE_DEPRECATED */

/**
 * VnckLayoutOrientation:
 * @VNCK_LAYOUT_ORIENTATION_HORIZONTAL: the #VnckWorkspace are laid out in
 * rows, with the first #VnckWorkspace in the defined #VnckLayoutCorner.
 * @VNCK_LAYOUT_ORIENTATION_VERTICAL: the #VnckWorkspace are laid out in
 * columns, with the first #VnckWorkspace in the defined #VnckLayoutCorner.
 *
 * Type defining the orientation of the layout of #VnckWorkspace. See
 * vnck_pager_set_orientation() for more information.
 */
typedef enum
{
  VNCK_LAYOUT_ORIENTATION_HORIZONTAL,
  VNCK_LAYOUT_ORIENTATION_VERTICAL
} _VnckLayoutOrientation;

/**
 * VnckLayoutCorner:
 * @VNCK_LAYOUT_CORNER_TOPLEFT: the first #VnckWorkspace is in the top left
 * corner of the layout.
 * @VNCK_LAYOUT_CORNER_TOPRIGHT: the first #VnckWorkspace is in the top right
 * corner of the layout.
 * @VNCK_LAYOUT_CORNER_BOTTOMRIGHT: the first #VnckWorkspace is in the bottom
 * right corner of the layout.
 * @VNCK_LAYOUT_CORNER_BOTTOMLEFT: the first #VnckWorkspace is in the bottom
 * left corner of the layout.
 *
 * Type defining the starting corner of the layout of #VnckWorkspace, i.e. the
 * corner containing the first #VnckWorkspace.
 */
typedef enum
{
  VNCK_LAYOUT_CORNER_TOPLEFT,
  VNCK_LAYOUT_CORNER_TOPRIGHT,
  VNCK_LAYOUT_CORNER_BOTTOMRIGHT,
  VNCK_LAYOUT_CORNER_BOTTOMLEFT
} _VnckLayoutCorner;

GType vnck_screen_get_type (void) G_GNUC_CONST;

VnckScreen*    vnck_screen_get_default              (void);
VnckScreen*    vnck_screen_get                      (int         index);
VnckScreen*    vnck_screen_get_for_root             (gulong      root_window_id);
int            vnck_screen_get_number               (VnckScreen *screen);
VnckWorkspace* vnck_screen_get_workspace            (VnckScreen *screen,
                                                     int         workspace);
VnckWorkspace* vnck_screen_get_active_workspace     (VnckScreen *screen);
GList*         vnck_screen_get_workspaces           (VnckScreen *screen);
VnckWindow*    vnck_screen_get_active_window        (VnckScreen *screen);
VnckWindow*    vnck_screen_get_previously_active_window (VnckScreen *screen);
GList*         vnck_screen_get_windows              (VnckScreen *screen);
GList*         vnck_screen_get_windows_stacked      (VnckScreen *screen);
void           vnck_screen_force_update             (VnckScreen *screen);
int            vnck_screen_get_workspace_count      (VnckScreen *screen);
void           vnck_screen_change_workspace_count   (VnckScreen *screen,
                                                     int         count);
const char*    vnck_screen_get_window_manager_name  (VnckScreen *screen);
gboolean       vnck_screen_net_wm_supports          (VnckScreen *screen,
                                                     const char *atom);
gulong         vnck_screen_get_background_pixmap    (VnckScreen *screen);
int            vnck_screen_get_width                (VnckScreen *screen);
int            vnck_screen_get_height               (VnckScreen *screen);
gboolean       vnck_screen_get_showing_desktop      (VnckScreen *screen);
void           vnck_screen_toggle_showing_desktop   (VnckScreen *screen,
                                                     gboolean    show);
void           vnck_screen_move_viewport            (VnckScreen *screen,
                                                     int         x,
                                                     int         y);
void           _vnck_screen_get_workspace_layout     (VnckScreen             *screen,
                                                      _VnckLayoutOrientation *orientation,
                                                      int                    *rows,
                                                      int                    *columns,
                                                      _VnckLayoutCorner      *starting_corner);
int            vnck_screen_try_set_workspace_layout (VnckScreen *screen,
                                                     int         current_token,
                                                     int         rows,
                                                     int         columns);
void           vnck_screen_release_workspace_layout (VnckScreen *screen,
                                                     int         current_token);
#ifndef VNCK_DISABLE_DEPRECATED
G_DEPRECATED
void           vnck_screen_calc_workspace_layout    (VnckScreen          *screen,
                                                     int                  num_workspaces,
                                                     int                  space_index,
                                                     VnckWorkspaceLayout *layout);
G_DEPRECATED
void           vnck_screen_free_workspace_layout (VnckWorkspaceLayout *layout);
#endif /* VNCK_DISABLE_DEPRECATED */


G_END_DECLS

#endif /* VNCK_SCREEN_H */
