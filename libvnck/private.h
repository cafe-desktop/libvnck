/* Private stuff */
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

#ifndef WNCK_PRIVATE_H
#define WNCK_PRIVATE_H

#include <config.h>
#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "application.h"
#include "xutils.h"
#include "pager.h"
#include "util.h"
#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

G_BEGIN_DECLS

#define WNCK_ACTIVATE_TIMEOUT 1

WnckClientType _vnck_get_client_type (void);

gsize _vnck_get_default_icon_size (void);
gsize _vnck_get_default_mini_icon_size (void);

void _vnck_application_process_property_notify (WnckApplication *app,
                                                XEvent          *xevent);
void _vnck_window_process_property_notify (WnckWindow *window,
                                           XEvent     *xevent);

void _vnck_screen_process_property_notify (WnckScreen *screen,
                                           XEvent     *xevent);
void _vnck_window_process_configure_notify (WnckWindow *window,
                                            XEvent     *xevent);
WnckWindow* _vnck_window_create  (Window      xwindow,
                                  WnckScreen *screen,
                                  gint        sort_order);
void        _vnck_window_destroy (WnckWindow *window);

void        _vnck_window_load_icons (WnckWindow *window);

char*       _vnck_window_get_name_for_display (WnckWindow *window,
                                               gboolean    use_icon_name,
                                               gboolean    use_state_decorations);
const char* _vnck_window_get_startup_id (WnckWindow *window);

time_t      _vnck_window_get_needs_attention_time (WnckWindow *window);
time_t      _vnck_window_or_transient_get_needs_attention_time (WnckWindow *window);

void        _vnck_window_shutdown_all (void);

WnckWorkspace* _vnck_workspace_create  (int            number, 
                                        WnckScreen    *screen);
void           _vnck_workspace_destroy (WnckWorkspace *space);

void _vnck_window_set_application    (WnckWindow      *window,
                                      WnckApplication *app);

void _vnck_window_set_class_group (WnckWindow     *window,
				   WnckClassGroup *class_group);

/* this one is in pager.c since it needs code from there to draw the icon */
void _vnck_window_set_as_drag_icon (WnckWindow     *window,
                                    GdkDragContext *context,
                                    GtkWidget      *drag_source);

void _vnck_application_add_window    (WnckApplication *app,
                                      WnckWindow      *window);
void _vnck_application_remove_window (WnckApplication *app,
                                      WnckWindow      *window);

WnckApplication* _vnck_application_create  (Window           xwindow,
                                            WnckScreen      *screen);
void             _vnck_application_destroy (WnckApplication *app);
void             _vnck_application_load_icons (WnckApplication *app);
void             _vnck_application_shutdown_all (void);

WnckClassGroup  *_vnck_class_group_create        (WnckScreen     *screen,
                                                  const char     *res_class);

void             _vnck_class_group_destroy       (WnckClassGroup *class_group);
void             _vnck_class_group_add_window    (WnckClassGroup *class_group,
                                                  WnckWindow     *window);
void             _vnck_class_group_remove_window (WnckClassGroup *class_group,
                                                  WnckWindow     *window);
void             _vnck_class_group_shutdown_all  (void);

void _vnck_workspace_update_name (WnckWorkspace *workspace,
                                  const char    *name);
void _vnck_screen_change_workspace_name (WnckScreen *screen,
                                         int         number,
                                         const char *name);

void _vnck_screen_shutdown_all          (void);

gboolean _vnck_workspace_set_geometry (WnckWorkspace *space, int w, int h);
gboolean _vnck_workspace_set_viewport (WnckWorkspace *space, int x, int y);

void _vnck_init (void);
Display *_vnck_get_default_display (void);

#define WNCK_SCREEN_XSCREEN(screen) (_vnck_screen_get_xscreen (screen))

Screen    *_vnck_screen_get_xscreen    (WnckScreen *screen);
GdkScreen *_vnck_screen_get_gdk_screen (WnckScreen *screen);

#ifdef HAVE_STARTUP_NOTIFICATION
SnDisplay* _vnck_screen_get_sn_display (WnckScreen *screen);
#endif

WnckScreen* _vnck_screen_get_existing (int number);

void           _vnck_pager_activate_workspace   (WnckWorkspace *wspace,
                                                 guint32        timestamp);
int            _vnck_pager_get_n_workspaces     (WnckPager     *pager);
const char*    _vnck_pager_get_workspace_name   (WnckPager     *pager,
                                                 int            i);
WnckWorkspace* _vnck_pager_get_active_workspace (WnckPager     *pager);
WnckWorkspace* _vnck_pager_get_workspace        (WnckPager     *pager,
                                                 int            i);
void           _vnck_pager_get_workspace_rect   (WnckPager     *pager,
                                                 int            i,
                                                 GdkRectangle  *rect);

void           _make_gtk_label_bold   (GtkLabel *label);
void           _make_gtk_label_normal (GtkLabel *label);

void           _vnck_selector_set_window_icon   (GtkWidget     *image,
                                                 WnckWindow    *window);

void           _vnck_ensure_fallback_style      (void);

G_END_DECLS

#endif /* WNCK_PRIVATE_H */