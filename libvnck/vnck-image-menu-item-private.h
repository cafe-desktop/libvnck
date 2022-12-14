/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VNCK_IMAGE_MENU_ITEM_PRIVATE_H
#define VNCK_IMAGE_MENU_ITEM_PRIVATE_H

#include <ctk/ctk.h>
#include "window.h"

G_BEGIN_DECLS

#define VNCK_TYPE_IMAGE_MENU_ITEM vnck_image_menu_item_get_type ()
G_DECLARE_FINAL_TYPE (VnckImageMenuItem, vnck_image_menu_item,
                      VNCK, IMAGE_MENU_ITEM, CtkMenuItem)

CtkWidget *vnck_image_menu_item_new                        (void);

CtkWidget *vnck_image_menu_item_new_with_label             (const gchar       *label);

void       vnck_image_menu_item_set_image_from_icon_pixbuf (VnckImageMenuItem *item,
                                                            GdkPixbuf         *pixbuf);

void       vnck_image_menu_item_set_image_from_window      (VnckImageMenuItem *item,
                                                            VnckWindow        *window);

void       vnck_image_menu_item_make_label_bold            (VnckImageMenuItem *item);

void       vnck_image_menu_item_make_label_normal          (VnckImageMenuItem *item);

G_END_DECLS

#endif
