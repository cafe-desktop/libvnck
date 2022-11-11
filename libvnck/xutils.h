/* Xlib utilities */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef VNCK_XUTILS_H
#define VNCK_XUTILS_H

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_APP_WINDOW_EVENT_MASK (PropertyChangeMask | StructureNotifyMask)

gboolean _vnck_get_cardinal      (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  int    *val);
int      _vnck_get_wm_state      (Screen *screen,
                                  Window  xwindow);
gboolean _vnck_get_window        (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  Window *val);
gboolean _vnck_get_pixmap        (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  Pixmap *val);
gboolean _vnck_get_atom          (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom,
                                  Atom   *val);
char*    _vnck_get_text_property (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom);
char*    _vnck_get_utf8_property (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom);
gboolean _vnck_get_window_list   (Screen  *screen,
                                  Window   xwindow,
                                  Atom     atom,
                                  Window **windows,
                                  int     *len);
gboolean _vnck_get_atom_list     (Screen  *screen,
                                  Window   xwindow,
                                  Atom     atom,
                                  Atom   **atoms,
                                  int     *len);
gboolean _vnck_get_cardinal_list (Screen  *screen,
                                  Window   xwindow,
                                  Atom     atom,
                                  gulong **cardinals,
                                  int     *len);
char**   _vnck_get_utf8_list     (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom);

void     _vnck_set_utf8_list     (Screen  *screen,
                                  Window   xwindow,
                                  Atom     atom,
                                  char   **list);

void _vnck_error_trap_push (Display *display);
int  _vnck_error_trap_pop  (Display *display);

#define _vnck_atom_get(atom_name) cdk_x11_get_xatom_by_name (atom_name)
#define _vnck_atom_name(atom)     cdk_x11_get_xatom_name (atom)

void _vnck_event_filter_init (void);
void _vnck_event_filter_shutdown (void);

int   _vnck_xid_equal (gconstpointer v1,
                       gconstpointer v2);
guint _vnck_xid_hash  (gconstpointer v);

void _vnck_iconify   (Screen *screen,
                      Window  xwindow);
void _vnck_deiconify (Screen *screen,
                      Window  xwindow);

void _vnck_close (VnckScreen *screen,
                  Window      xwindow,
                  Time        timestamp);

void _vnck_change_state (VnckScreen *screen,
                         Window      xwindow,
                         gboolean    add,
                         Atom        state1,
                         Atom        state2);

void _vnck_change_workspace (VnckScreen *screen,
                             Window      xwindow,
                             int         new_space);

void _vnck_activate (VnckScreen *screen,
                     Window      xwindow,
                     Time        timestamp);

void _vnck_activate_workspace (Screen *screen,
                               int     new_active_space,
                               Time    timestamp);
void _vnck_change_viewport (Screen *screen,
			    int     x,
			    int     y);

char*  _vnck_get_session_id     (Screen *screen,
                                 Window xwindow);
int    _vnck_get_pid            (Screen *screen,
                                 Window  xwindow);
char*  _vnck_get_name           (Screen *screen,
                                 Window  xwindow);
char*  _vnck_get_icon_name      (Screen *screen,
                                 Window  xwindow);
char*  _vnck_get_res_class_utf8 (Screen *screen,
                                 Window  xwindow);
void   _vnck_get_wmclass        (Screen *screen,
                                 Window  xwindow,
                                 char **res_class,
                                 char **res_name);
gboolean _vnck_get_frame_extents  (Screen *screen,
                                   Window  xwindow,
                                   int    *left_frame,
                                   int    *right_frame,
                                   int    *top_frame,
                                   int    *bottom_frame);

int    _vnck_select_input     (Screen  *screen,
                               Window   xwindow,
                               int      mask,
                               gboolean update);

void _vnck_keyboard_move (VnckScreen *screen,
                          Window      xwindow);

void _vnck_keyboard_size (VnckScreen *screen,
                          Window      xwindow);

void _vnck_toggle_showing_desktop (Screen  *screen,
                                   gboolean show);

typedef struct _VnckIconCache VnckIconCache;

VnckIconCache *_vnck_icon_cache_new                  (void);
void           _vnck_icon_cache_free                 (VnckIconCache *icon_cache);
void           _vnck_icon_cache_property_changed     (VnckIconCache *icon_cache,
                                                      Atom           atom);
gboolean       _vnck_icon_cache_get_icon_invalidated (VnckIconCache *icon_cache);
void           _vnck_icon_cache_set_want_fallback    (VnckIconCache *icon_cache,
                                                      gboolean       setting);
gboolean       _vnck_icon_cache_get_is_fallback      (VnckIconCache *icon_cache);

gboolean _vnck_read_icons (VnckScreen     *screen,
                           Window          xwindow,
                           VnckIconCache  *icon_cache,
                           CdkPixbuf     **iconp,
                           int             ideal_width,
                           int             ideal_height,
                           CdkPixbuf     **mini_iconp,
                           int             ideal_mini_width,
                           int             ideal_mini_height);

void _vnck_get_fallback_icons (CdkPixbuf     **iconp,
                               int             ideal_width,
                               int             ideal_height,
                               CdkPixbuf     **mini_iconp,
                               int             ideal_mini_width,
                               int             ideal_mini_height);



void _vnck_get_window_geometry (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp,
                                int    *widthp,
                                int    *heightp);
void _vnck_set_window_geometry (Screen *screen,
                                Window  xwindow,
                                int     gravity_and_flags,
                                int     x,
                                int     y,
                                int     width,
                                int     height);

void _vnck_get_window_position (Screen *screen,
				Window  xwindow,
                                int    *xp,
                                int    *yp);

void _vnck_set_icon_geometry  (Screen *screen,
                               Window  xwindow,
			       int     x,
			       int     y,
			       int     width,
			       int     height);

void _vnck_set_desktop_layout (Screen *xscreen,
                               int     rows,
                               int     columns);

CdkPixbuf* _vnck_cdk_pixbuf_get_from_pixmap (Screen *screen,
                                             Pixmap  xpixmap);

CdkDisplay* _vnck_cdk_display_lookup_from_display (Display *display);

CdkWindow* _vnck_cdk_window_lookup_from_window (Screen *screen,
                                                Window  xwindow);

#define VNCK_NO_MANAGER_TOKEN 0

int      _vnck_try_desktop_layout_manager           (Screen *xscreen,
                                                     int     current_token);
void     _vnck_release_desktop_layout_manager       (Screen *xscreen,
                                                     int     current_token);
gboolean _vnck_desktop_layout_manager_process_event (XEvent *xev);

G_END_DECLS

#endif /* VNCK_XUTILS_H */
