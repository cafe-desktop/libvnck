/* tasklist object */
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

#ifndef VNCK_TASKLIST_H
#define VNCK_TASKLIST_H

#include <ctk/ctk.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_TYPE_TASKLIST              (vnck_tasklist_get_type ())
#define VNCK_TASKLIST(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_TASKLIST, VnckTasklist))
#define VNCK_TASKLIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_TASKLIST, VnckTasklistClass))
#define VNCK_IS_TASKLIST(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_TASKLIST))
#define VNCK_IS_TASKLIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_TASKLIST))
#define VNCK_TASKLIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_TASKLIST, VnckTasklistClass))

typedef struct _VnckTasklist        VnckTasklist;
typedef struct _VnckTasklistClass   VnckTasklistClass;
typedef struct _VnckTasklistPrivate VnckTasklistPrivate;

/**
 * VnckTasklist:
 *
 * The #VnckTasklist struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckTasklist
{
  CtkContainer parent_instance;

  VnckTasklistPrivate *priv;
};

struct _VnckTasklistClass
{
  CtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * VnckTasklistGroupingType:
 * @VNCK_TASKLIST_NEVER_GROUP: never group multiple #VnckWindow of the same
 * #VnckApplication.
 * @VNCK_TASKLIST_AUTO_GROUP: group multiple #VnckWindow of the same
 * #VnckApplication for some #VnckApplication, when there is not enough place
 * to have a good-looking list of all #VnckWindow.
 * @VNCK_TASKLIST_ALWAYS_GROUP: always group multiple #VnckWindow of the same
 * #VnckApplication, for all #VnckApplication.
 *
 * Type defining the policy of the #VnckTasklist for grouping multiple
 * #VnckWindow of the same #VnckApplication.
 */
typedef enum {
  VNCK_TASKLIST_NEVER_GROUP,
  VNCK_TASKLIST_AUTO_GROUP,
  VNCK_TASKLIST_ALWAYS_GROUP
} VnckTasklistGroupingType;

GType vnck_tasklist_get_type (void) G_GNUC_CONST;

CtkWidget *vnck_tasklist_new (void);
const int *vnck_tasklist_get_size_hint_list (VnckTasklist  *tasklist,
					      int           *n_elements);

void vnck_tasklist_set_grouping (VnckTasklist             *tasklist,
				 VnckTasklistGroupingType  grouping);
void vnck_tasklist_set_switch_workspace_on_unminimize (VnckTasklist  *tasklist,
						       gboolean       switch_workspace_on_unminimize);
void vnck_tasklist_set_middle_click_close (VnckTasklist  *tasklist,
					   gboolean       middle_click_close);
void vnck_tasklist_set_grouping_limit (VnckTasklist *tasklist,
				       gint          limit);
void vnck_tasklist_set_include_all_workspaces (VnckTasklist *tasklist,
					       gboolean      include_all_workspaces);
void vnck_tasklist_set_button_relief (VnckTasklist *tasklist,
                                      CtkReliefStyle relief);
void vnck_tasklist_set_orientation (VnckTasklist *tasklist,
                                    CtkOrientation orient);
void vnck_tasklist_set_scroll_enabled (VnckTasklist *tasklist,
                                       gboolean      scroll_enabled);
gboolean vnck_tasklist_get_scroll_enabled (VnckTasklist *tasklist);

/**
 * VnckLoadIconFunction:
 * @icon_name: an icon name as in the Icon field in a .desktop file for the
 * icon to load.
 * @size: the desired icon size.
 * @flags: not defined to do anything yet.
 * @data: data passed to the function, set when the #VnckLoadIconFunction has
 * been set for the #VnckTasklist.
 *
 * Specifies the type of function passed to vnck_tasklist_set_icon_loader().
 *
 * Returns: it should return a <classname>CdkPixbuf</classname> of @icon_name
 * at size @size, or %NULL if no icon for @icon_name at size @size could be
 * loaded.
 *
 * Since: 2.2
 */
typedef CdkPixbuf* (*VnckLoadIconFunction) (const char   *icon_name,
                                            int           size,
                                            unsigned int  flags,
                                            void         *data);

void vnck_tasklist_set_icon_loader (VnckTasklist         *tasklist,
                                    VnckLoadIconFunction  load_icon_func,
                                    void                 *data,
                                    GDestroyNotify        free_data_func);

G_END_DECLS

#endif /* VNCK_TASKLIST_H */
