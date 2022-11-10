/* workspace object */
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

#ifndef VNCK_WORKSPACE_H
#define VNCK_WORKSPACE_H

#include <glib-object.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_TYPE_WORKSPACE              (vnck_workspace_get_type ())
#define VNCK_WORKSPACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_WORKSPACE, VnckWorkspace))
#define VNCK_WORKSPACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_WORKSPACE, VnckWorkspaceClass))
#define VNCK_IS_WORKSPACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_WORKSPACE))
#define VNCK_IS_WORKSPACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_WORKSPACE))
#define VNCK_WORKSPACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_WORKSPACE, VnckWorkspaceClass))

typedef struct _VnckWorkspaceClass   VnckWorkspaceClass;
typedef struct _VnckWorkspacePrivate VnckWorkspacePrivate;

/**
 * VnckWorkspace:
 *
 * The #VnckWorkspace struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckWorkspace
{
  GObject parent_instance;

  VnckWorkspacePrivate *priv;
};

struct _VnckWorkspaceClass
{
  GObjectClass parent_class;

  void (* name_changed) (VnckWorkspace *space);
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * VnckMotionDirection:
 * @VNCK_MOTION_UP: search a neighbor #VnckWorkspace above another
 * #VnckWorkspace. 
 * @VNCK_MOTION_DOWN: search a neighbor #VnckWorkspace below another
 * #VnckWorkspace.
 * @VNCK_MOTION_LEFT: search a neighbor #VnckWorkspace at the left of another
 * #VnckWorkspace.
 * @VNCK_MOTION_RIGHT: search a neighbor #VnckWorkspace at the right of another
 * #VnckWorkspace.
 *
 * Type defining a direction in which to search a neighbor #VnckWorkspace.
 *
 * Since: 2.14
 */
typedef enum
{
  VNCK_MOTION_UP = -1,
  VNCK_MOTION_DOWN = -2,
  VNCK_MOTION_LEFT = -3,
  VNCK_MOTION_RIGHT = -4
} VnckMotionDirection;

GType vnck_workspace_get_type (void) G_GNUC_CONST;

int         vnck_workspace_get_number     (VnckWorkspace *space);
const char* vnck_workspace_get_name       (VnckWorkspace *space);
void        vnck_workspace_change_name    (VnckWorkspace *space,
                                           const char    *name);
VnckScreen* vnck_workspace_get_screen     (VnckWorkspace *space);
void        vnck_workspace_activate       (VnckWorkspace *space,
                                           guint32        timestamp);
int         vnck_workspace_get_width      (VnckWorkspace *space);
int         vnck_workspace_get_height     (VnckWorkspace *space);
int         vnck_workspace_get_viewport_x (VnckWorkspace *space);
int         vnck_workspace_get_viewport_y (VnckWorkspace *space);
gboolean    vnck_workspace_is_virtual     (VnckWorkspace *space);


int vnck_workspace_get_layout_row          (VnckWorkspace       *space);
int vnck_workspace_get_layout_column       (VnckWorkspace       *space);
VnckWorkspace* vnck_workspace_get_neighbor (VnckWorkspace       *space,
                                            VnckMotionDirection  direction);

G_END_DECLS

#endif /* VNCK_WORKSPACE_H */
