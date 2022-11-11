/* pager object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003, 2005-2007 Vincent Untz
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

#ifndef VNCK_PAGER_H
#define VNCK_PAGER_H

#include <ctk/ctk.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_TYPE_PAGER              (vnck_pager_get_type ())
#define VNCK_PAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_PAGER, VnckPager))
#define VNCK_PAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_PAGER, VnckPagerClass))
#define VNCK_IS_PAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_PAGER))
#define VNCK_IS_PAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_PAGER))
#define VNCK_PAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_PAGER, VnckPagerClass))

typedef struct _VnckPager        VnckPager;
typedef struct _VnckPagerClass   VnckPagerClass;
typedef struct _VnckPagerPrivate VnckPagerPrivate;

/**
 * VnckPager:
 *
 * The #VnckPager struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckPager
{
  GtkContainer parent_instance;

  VnckPagerPrivate *priv;
};

struct _VnckPagerClass
{
  GtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * VnckPagerDisplayMode:
 * @VNCK_PAGER_DISPLAY_NAME: the #VnckPager will only display the names of the
 * workspaces.
 * @VNCK_PAGER_DISPLAY_CONTENT: the #VnckPager will display a representation
 * for each window in the workspaces.
 *
 * Mode defining what a #VnckPager will display.
 */
typedef enum {
  VNCK_PAGER_DISPLAY_NAME,
  VNCK_PAGER_DISPLAY_CONTENT
} VnckPagerDisplayMode;

/**
 * VnckPagerScrollMode:
 * @VNCK_PAGER_SCROLL_2D: given that the workspaces are set up in multiple rows,
 * scrolling on the #VnckPager will cycle through the workspaces as if on a
 * 2-dimensional map. Example cycling order with 2 rows and 4 workspaces: 1 3 2 4.
 * @VNCK_PAGER_SCROLL_1D: the #VnckPager will always cycle workspaces in a linear
 * manner, irrespective of how many rows are configured. (Hint: Better for mice)
 * Example cycling order with 2 rows and 4 workspaces: 1 2 3 4.
 *
 * Mode defining in which order scrolling on a #VnckPager will cycle through workspaces.
 */
typedef enum {
  VNCK_PAGER_SCROLL_2D,
  VNCK_PAGER_SCROLL_1D
} VnckPagerScrollMode;

GType      vnck_pager_get_type           (void) G_GNUC_CONST;

GtkWidget* vnck_pager_new                (void);

gboolean   vnck_pager_set_orientation    (VnckPager            *pager,
                                          GtkOrientation        orientation);
gboolean   vnck_pager_set_n_rows         (VnckPager            *pager,
                                          int                   n_rows);
void       vnck_pager_set_display_mode   (VnckPager            *pager,
                                          VnckPagerDisplayMode  mode);
void       vnck_pager_set_scroll_mode    (VnckPager            *pager,
                                          VnckPagerScrollMode   scroll_mode);
void       vnck_pager_set_show_all       (VnckPager            *pager,
                                          gboolean              show_all_workspaces);
void       vnck_pager_set_shadow_type    (VnckPager            *pager,
                                          GtkShadowType         shadow_type);
void       vnck_pager_set_wrap_on_scroll (VnckPager            *pager,
                                          gboolean              wrap_on_scroll);
gboolean   vnck_pager_get_wrap_on_scroll (VnckPager            *pager);

G_END_DECLS

#endif /* VNCK_PAGER_H */
