/* workspace object */
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
#include "workspace.h"
#include "xutils.h"
#include "private.h"
#include <string.h>

/**
 * SECTION:workspace
 * @short_description: an object representing a workspace.
 * @see_also: #VnckScreen
 * @stability: Unstable
 *
 * The #VnckWorkspace represents what is called <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html&num;largedesks">virtual
 * desktops</ulink> in the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>.
 * A workspace is a virtualization of a #VnckScreen<!-- -->: only one workspace
 * can be shown on a #VnckScreen at a time. It makes it possible, for example,
 * to put windows on different workspaces to organize them.
 *
 * If the #VnckWorkspace size is bigger that the #VnckScreen size, the
 * workspace contains a viewport. Viewports are defined in the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html&num;id2457064">large
 * desktops</ulink> section of the <ulink
 * url="http://standards.freedesktop.org/wm-spec/wm-spec-latest.html">EWMH</ulink>.
 * The notion of workspaces and viewports are quite similar, and generally both
 * are not used at the same time: there are generally either multiple
 * workspaces with no viewport, or one workspace with a viewport. libvnck
 * supports all situations, even multiple workspaces with viewports.
 *
 * Workspaces are organized according to a layout set on the #VnckScreen. See
 * vnck_screen_try_set_workspace_layout() and
 * vnck_screen_release_workspace_layout() for more information about the
 * layout.
 *
 * The #VnckWorkspace objects are always owned by libvnck and must not be
 * referenced or unreferenced.
 */

struct _VnckWorkspacePrivate
{
  VnckScreen *screen;
  int number;
  char *name;
  int width, height;            /* Workspace size */
  int viewport_x, viewport_y;   /* Viewport origin */
  gboolean is_virtual;
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckWorkspace, vnck_workspace, G_TYPE_OBJECT);

enum {
  NAME_CHANGED,
  LAST_SIGNAL
};

static void vnck_workspace_finalize    (GObject        *object);

static void emit_name_changed (VnckWorkspace *space);

static guint signals[LAST_SIGNAL] = { 0 };

static void
vnck_workspace_init (VnckWorkspace *workspace)
{
  workspace->priv = vnck_workspace_get_instance_private (workspace);

  workspace->priv->number = -1;
}

static void
vnck_workspace_class_init (VnckWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vnck_workspace_finalize;

  /**
   * VnckWorkspace::name-changed:
   * @space: the #VnckWorkspace which emitted the signal.
   *
   * Emitted when the name of @space changes.
   */
  signals[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VnckWorkspaceClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
vnck_workspace_finalize (GObject *object)
{
  VnckWorkspace *workspace;

  workspace = VNCK_WORKSPACE (object);

  g_free (workspace->priv->name);
  workspace->priv->name = NULL;

  G_OBJECT_CLASS (vnck_workspace_parent_class)->finalize (object);
}

/**
 * vnck_workspace_get_number:
 * @space: a #VnckWorkspace.
 *
 * Gets the index of @space on the #VnckScreen to which it belongs. The
 * first workspace has an index of 0.
 *
 * Return value: the index of @space on its #VnckScreen, or -1 on errors.
 **/
int
vnck_workspace_get_number (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), -1);

  return space->priv->number;
}

/**
 * vnck_workspace_get_name:
 * @space: a #VnckWorkspace.
 *
 * Gets the human-readable name that should be used to refer to @space. If
 * the user has not set a special name, a fallback like "Workspace 3" will be
 * used.
 *
 * Return value: the name of @space.
 **/
const char*
vnck_workspace_get_name (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), NULL);

  return space->priv->name;
}

/**
 * vnck_workspace_change_name:
 * @space: a #VnckWorkspace.
 * @name: new name for @space.
 *
 * Changes the name of @space.
 *
 * Since: 2.2
 **/
void
vnck_workspace_change_name (VnckWorkspace *space,
                            const char    *name)
{
  g_return_if_fail (VNCK_IS_WORKSPACE (space));
  g_return_if_fail (name != NULL);

  _vnck_screen_change_workspace_name (space->priv->screen,
                                      space->priv->number,
                                      name);
}

/**
 * vnck_workspace_get_screen:
 * @space: a #VnckWorkspace.
 *
 * Gets the #VnckScreen @space is on.
 *
 * Return value: (transfer none): the #VnckScreen @space is on. The returned
 * #VnckScreen is owned by libvnck and must not be referenced or unreferenced.
 **/
VnckScreen*
vnck_workspace_get_screen (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), NULL);

  return space->priv->screen;
}

/**
 * vnck_workspace_activate:
 * @space: a #VnckWorkspace.
 * @timestamp: the X server timestamp of the user interaction event that caused
 * this call to occur.
 *
 * Asks the window manager to make @space the active workspace. The window
 * manager may decide to refuse the request (to not steal the focus if there is
 * a more recent user activity, for example).
 *
 * This function existed before 2.10, but the @timestamp argument was missing
 * in earlier versions.
 *
 * Since: 2.10
 **/
void
vnck_workspace_activate (VnckWorkspace *space,
                         guint32        timestamp)
{
  g_return_if_fail (VNCK_IS_WORKSPACE (space));

  _vnck_activate_workspace (VNCK_SCREEN_XSCREEN (space->priv->screen),
                            space->priv->number,
                            timestamp);
}

VnckWorkspace*
_vnck_workspace_create (int number, VnckScreen *screen)
{
  VnckWorkspace *space;

  space = g_object_new (VNCK_TYPE_WORKSPACE, NULL);
  space->priv->number = number;
  space->priv->name = NULL;
  space->priv->screen = screen;

  _vnck_workspace_update_name (space, NULL);

  /* Just set reasonable defaults */
  space->priv->width = vnck_screen_get_width (screen);
  space->priv->height = vnck_screen_get_height (screen);
  space->priv->is_virtual = FALSE;

  space->priv->viewport_x = 0;
  space->priv->viewport_y = 0;

  return space;
}

void
_vnck_workspace_update_name (VnckWorkspace *space,
                             const char    *name)
{
  char *old;

  g_return_if_fail (VNCK_IS_WORKSPACE (space));

  old = space->priv->name;
  space->priv->name = g_strdup (name);

  if (space->priv->name == NULL)
    space->priv->name = g_strdup_printf (_("Workspace %d"),
                                         space->priv->number + 1);

  if ((old && !name) ||
      (!old && name) ||
      (old && name && strcmp (old, name) != 0))
    emit_name_changed (space);

  g_free (old);
}

static void
emit_name_changed (VnckWorkspace *space)
{
  g_signal_emit (G_OBJECT (space),
                 signals[NAME_CHANGED],
                 0);
}

gboolean
_vnck_workspace_set_geometry (VnckWorkspace *space,
                              int            w,
                              int            h)
{
  if (space->priv->width != w || space->priv->height != h)
    {
      space->priv->width = w;
      space->priv->height = h;

      space->priv->is_virtual = w > vnck_screen_get_width (space->priv->screen) ||
				h > vnck_screen_get_height (space->priv->screen);

      return TRUE;  /* change was made */
    }
  else
    return FALSE;
}

gboolean
_vnck_workspace_set_viewport (VnckWorkspace *space,
                              int            x,
                              int            y)
{
  if (space->priv->viewport_x != x || space->priv->viewport_y != y)
    {
      space->priv->viewport_x = x;
      space->priv->viewport_y = y;

      return TRUE; /* change was made */
    }
  else
    return FALSE;
}

/**
 * vnck_workspace_get_width:
 * @space: a #VnckWorkspace.
 *
 * Gets the width of @space.
 *
 * Returns: the width of @space.
 *
 * Since: 2.4
 */
int
vnck_workspace_get_width (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), 0);

  return space->priv->width;
}

/**
 * vnck_workspace_get_height:
 * @space: a #VnckWorkspace.
 *
 * Gets the height of @space.
 *
 * Returns: the height of @space.
 *
 * Since: 2.4
 */
int
vnck_workspace_get_height (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), 0);

  return space->priv->height;
}

/**
 * vnck_workspace_get_viewport_x:
 * @space: a #VnckWorkspace.
 *
 * Gets the X coordinate of the viewport in @space.
 *
 * Returns: the X coordinate of the viewport in @space, or 0 if @space does not
 * contain a viewport.
 *
 * Since: 2.4
 */
int
vnck_workspace_get_viewport_x (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), 0);

  return space->priv->viewport_x;
}

/**
 * vnck_workspace_get_viewport_y:
 * @space: a #VnckWorkspace.
 *
 * Gets the Y coordinate of the viewport in @space.
 *
 * Returns: the Y coordinate of the viewport in @space, or 0 if @space does not
 * contain a viewport.
 *
 * Since: 2.4
 */
int
vnck_workspace_get_viewport_y (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), 0);

  return space->priv->viewport_y;
}

/**
 * vnck_workspace_is_virtual:
 * @space: a #VnckWorkspace.
 *
 * Gets whether @space contains a viewport.
 *
 * Returns: %TRUE if @space contains a viewport, %FALSE otherwise.
 *
 * Since: 2.4
 */
gboolean
vnck_workspace_is_virtual (VnckWorkspace *space)
{
  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), FALSE);

  return space->priv->is_virtual;
}

/**
 * vnck_workspace_get_layout_row:
 * @space: a #VnckWorkspace.
 *
 * Gets the row of @space in the #VnckWorkspace layout. The first row has an
 * index of 0 and is always the top row, regardless of the starting corner set
 * for the layout.
 *
 * Return value: the row of @space in the #VnckWorkspace layout, or -1 on
 * errors.
 *
 * Since: 2.20
 **/
int
vnck_workspace_get_layout_row (VnckWorkspace *space)
{
  _VnckLayoutOrientation orientation;
  _VnckLayoutCorner corner;
  int n_rows;
  int n_cols;
  int row;

  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), -1);

  _vnck_screen_get_workspace_layout (space->priv->screen,
                                     &orientation, &n_rows, &n_cols, &corner);

  if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
    row = space->priv->number / n_cols;
  else
    row = space->priv->number % n_rows;

  if (corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT ||
      corner == VNCK_LAYOUT_CORNER_BOTTOMLEFT)
    row = n_rows - row;

  return row;
}

/**
 * vnck_workspace_get_layout_column:
 * @space: a #VnckWorkspace.
 *
 * Gets the column of @space in the #VnckWorkspace layout. The first column
 * has an index of 0 and is always the left column, regardless of the starting
 * corner set for the layout and regardless of the default direction of the
 * environment (i.e., in both Left-To-Right and Right-To-Left environments).
 *
 * Return value: the column of @space in the #VnckWorkspace layout, or -1 on
 * errors.
 *
 * Since: 2.20
 **/
int
vnck_workspace_get_layout_column (VnckWorkspace *space)
{
  _VnckLayoutOrientation orientation;
  _VnckLayoutCorner corner;
  int n_rows;
  int n_cols;
  int col;

  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), -1);

  _vnck_screen_get_workspace_layout (space->priv->screen,
                                     &orientation, &n_rows, &n_cols, &corner);

  if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
    col = space->priv->number % n_cols;
  else
    col = space->priv->number / n_rows;

  if (corner == VNCK_LAYOUT_CORNER_TOPRIGHT ||
      corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT)
    col = n_cols - col;

  return col;
}

/**
 * vnck_workspace_get_neighbor:
 * @space: a #VnckWorkspace.
 * @direction: direction in which to search the neighbor.
 *
 * Gets the neighbor #VnckWorkspace of @space in the @direction direction.
 *
 * Return value: (transfer none): the neighbor #VnckWorkspace of @space in the
 * @direction direction, or %NULL if no such neighbor #VnckWorkspace exists.
 * The returned #VnckWorkspace is owned by libvnck and must not be referenced
 * or unreferenced.
 *
 * Since: 2.20
 **/
VnckWorkspace*
vnck_workspace_get_neighbor (VnckWorkspace       *space,
                             VnckMotionDirection  direction)
{
  _VnckLayoutOrientation orientation;
  _VnckLayoutCorner corner;
  int n_rows;
  int n_cols;
  int row;
  int col;
  int add;
  int index;

  g_return_val_if_fail (VNCK_IS_WORKSPACE (space), NULL);

  _vnck_screen_get_workspace_layout (space->priv->screen,
                                     &orientation, &n_rows, &n_cols, &corner);

  row = vnck_workspace_get_layout_row (space);
  col = vnck_workspace_get_layout_column (space);

  index = space->priv->number;

  switch (direction)
    {
    case VNCK_MOTION_LEFT:
      if (col == 0)
        return NULL;

      if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
        add = 1;
      else
        add = n_rows;

      if (corner == VNCK_LAYOUT_CORNER_TOPRIGHT ||
          corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT)
        index += add;
      else
        index -= add;
      break;

    case VNCK_MOTION_RIGHT:
      if (col == n_cols - 1)
        return NULL;

      if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
        add = 1;
      else
        add = n_rows;

      if (corner == VNCK_LAYOUT_CORNER_TOPRIGHT ||
          corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT)
        index -= add;
      else
        index += add;
      break;

    case VNCK_MOTION_UP:
      if (row == 0)
        return NULL;

      if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
        add = n_cols;
      else
        add = 1;

      if (corner == VNCK_LAYOUT_CORNER_BOTTOMLEFT ||
          corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT)
        index += add;
      else
        index -= add;
      break;

    case VNCK_MOTION_DOWN:
      if (row == n_rows - 1)
        return NULL;

      if (orientation == VNCK_LAYOUT_ORIENTATION_HORIZONTAL)
        add = n_cols;
      else
        add = 1;

      if (corner == VNCK_LAYOUT_CORNER_BOTTOMLEFT ||
          corner == VNCK_LAYOUT_CORNER_BOTTOMRIGHT)
        index -= add;
      else
        index += add;
      break;

    default:
      break;
    }

  if (index == space->priv->number)
    return NULL;

  return vnck_screen_get_workspace (space->priv->screen, index);
}
