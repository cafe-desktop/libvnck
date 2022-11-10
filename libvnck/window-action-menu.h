/* window action menu (ops on a single window) */
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

#ifndef VNCK_WINDOW_ACTION_MENU_H
#define VNCK_WINDOW_ACTION_MENU_H

#include <libvnck/window.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VNCK_TYPE_ACTION_MENU              (vnck_action_menu_get_type ())
#define VNCK_ACTION_MENU(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_ACTION_MENU, WnckActionMenu))
#define VNCK_ACTION_MENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_ACTION_MENU, WnckActionMenuClass))
#define VNCK_IS_ACTION_MENU(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_ACTION_MENU))
#define VNCK_IS_ACTION_MENU_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_ACTION_MENU))
#define VNCK_ACTION_MENU_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_ACTION_MENU, WnckActionMenuClass))

typedef struct _WnckActionMenu        WnckActionMenu;
typedef struct _WnckActionMenuClass   WnckActionMenuClass;
typedef struct _WnckActionMenuPrivate WnckActionMenuPrivate;

/**
 * WnckActionMenu:
 *
 * The #WnckActionMenu struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckActionMenu
{
  GtkMenu parent_instance;

  WnckActionMenuPrivate *priv;
};

struct _WnckActionMenuClass
{
  GtkMenuClass parent_class;

  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType vnck_action_menu_get_type (void) G_GNUC_CONST;

GtkWidget* vnck_action_menu_new (WnckWindow *window);

G_END_DECLS

#endif /* VNCK_WINDOW_MENU_H */
