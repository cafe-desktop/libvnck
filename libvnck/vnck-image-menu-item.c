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

#include "config.h"

#include "vnck-image-menu-item-private.h"
#include "private.h"

#define SPACING 6

struct _VnckImageMenuItem
{
  GtkMenuItem  parent;

  GtkWidget   *box;
  GtkWidget   *image;
  GtkWidget   *accel_label;

  gchar       *label;
};

G_DEFINE_TYPE (VnckImageMenuItem, vnck_image_menu_item, CTK_TYPE_MENU_ITEM)

static void
vnck_image_menu_item_finalize (GObject *object)
{
  VnckImageMenuItem *item;

  item = VNCK_IMAGE_MENU_ITEM (object);

  g_clear_pointer (&item->label, g_free);

  G_OBJECT_CLASS (vnck_image_menu_item_parent_class)->finalize (object);
}

static void
vnck_image_menu_item_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
  GtkWidgetClass *widget_class;
  VnckImageMenuItem *item;
  GtkRequisition image_requisition;

  widget_class = CTK_WIDGET_CLASS (vnck_image_menu_item_parent_class);
  item = VNCK_IMAGE_MENU_ITEM (widget);

  widget_class->get_preferred_width (widget, minimum, natural);

  if (!ctk_widget_get_visible (item->image))
    return;

  ctk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  if (image_requisition.width > 0)
    {
      *minimum -= image_requisition.width + SPACING;
      *natural -= image_requisition.width + SPACING;
    }
}

static void
vnck_image_menu_item_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *allocation)
{
  GtkWidgetClass *widget_class;
  VnckImageMenuItem *item;
  GtkRequisition image_requisition;
  GtkAllocation box_allocation;

  widget_class = CTK_WIDGET_CLASS (vnck_image_menu_item_parent_class);
  item = VNCK_IMAGE_MENU_ITEM (widget);

  widget_class->size_allocate (widget, allocation);

  if (!ctk_widget_get_visible (item->image))
    return;

  ctk_widget_get_preferred_size (item->image, &image_requisition, NULL);
  ctk_widget_get_allocation (item->box, &box_allocation);

  if (ctk_widget_get_direction (widget) == CTK_TEXT_DIR_LTR)
    {
      if (image_requisition.width > 0)
        box_allocation.x -= image_requisition.width + SPACING;
    }
  else
    {
      if (image_requisition.width > 0)
        box_allocation.x += image_requisition.width + SPACING;
    }

  ctk_widget_size_allocate (item->box, &box_allocation);
}

static const gchar *
vnck_image_menu_item_get_label (GtkMenuItem *menu_item)
{
  VnckImageMenuItem *item;

  item = VNCK_IMAGE_MENU_ITEM (menu_item);

  return item->label;
}

static void
vnck_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                          gint        *requisition)
{
  VnckImageMenuItem *item;
  GtkRequisition image_requisition;

  item = VNCK_IMAGE_MENU_ITEM (menu_item);

  *requisition = 0;

  if (!ctk_widget_get_visible (item->image))
    return;

  ctk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  if (image_requisition.width > 0)
    *requisition = image_requisition.width + SPACING;
}

static void
vnck_image_menu_item_set_label (GtkMenuItem *menu_item,
                                const gchar *label)
{
  VnckImageMenuItem *item;

  item = VNCK_IMAGE_MENU_ITEM (menu_item);

  if (g_strcmp0 (item->label, label) != 0)
    {
      g_free (item->label);
      item->label = g_strdup (label);

      ctk_label_set_text_with_mnemonic (CTK_LABEL (item->accel_label), label);
      g_object_notify (G_OBJECT (menu_item), "label");
    }
}

static void
vnck_image_menu_item_class_init (VnckImageMenuItemClass *item_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  object_class = G_OBJECT_CLASS (item_class);
  widget_class = CTK_WIDGET_CLASS (item_class);
  menu_item_class = CTK_MENU_ITEM_CLASS (item_class);

  object_class->finalize = vnck_image_menu_item_finalize;

  widget_class->get_preferred_width = vnck_image_menu_item_get_preferred_width;
  widget_class->size_allocate = vnck_image_menu_item_size_allocate;

  menu_item_class->get_label = vnck_image_menu_item_get_label;
  menu_item_class->toggle_size_request = vnck_image_menu_item_toggle_size_request;
  menu_item_class->set_label = vnck_image_menu_item_set_label;
}

static void
vnck_image_menu_item_init (VnckImageMenuItem *item)
{
  GtkAccelLabel *accel_label;

  item->box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, SPACING);
  ctk_container_add (CTK_CONTAINER (item), item->box);
  ctk_widget_show (item->box);

  item->image = ctk_image_new ();
  ctk_box_pack_start (CTK_BOX (item->box), item->image, FALSE, FALSE, 0);

  item->accel_label = ctk_accel_label_new ("");
  ctk_box_pack_end (CTK_BOX (item->box), item->accel_label, TRUE, TRUE, 0);
  ctk_label_set_xalign (CTK_LABEL (item->accel_label), 0.0);
  ctk_widget_show (item->accel_label);

  accel_label = CTK_ACCEL_LABEL (item->accel_label);
  ctk_accel_label_set_accel_widget (accel_label, CTK_WIDGET (item));
  ctk_label_set_ellipsize (CTK_LABEL (accel_label), PANGO_ELLIPSIZE_END);
  ctk_label_set_use_underline (CTK_LABEL (accel_label), TRUE);
}

GtkWidget *
vnck_image_menu_item_new (void)
{
  return g_object_new (VNCK_TYPE_IMAGE_MENU_ITEM, NULL);
}

GtkWidget *
vnck_image_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (VNCK_TYPE_IMAGE_MENU_ITEM, "label", label, NULL);
}

void
vnck_image_menu_item_set_image_from_icon_pixbuf (VnckImageMenuItem *item,
                                                 GdkPixbuf         *pixbuf)
{
  ctk_image_set_from_pixbuf (CTK_IMAGE (item->image), pixbuf);
  ctk_widget_show (item->image);
}

void
vnck_image_menu_item_set_image_from_window (VnckImageMenuItem *item,
                                            VnckWindow        *window)
{
  _vnck_selector_set_window_icon (item->image, window);
  ctk_widget_show (item->image);
}

void
vnck_image_menu_item_make_label_bold (VnckImageMenuItem *item)
{
  _make_ctk_label_bold (CTK_LABEL (item->accel_label));
}

void
vnck_image_menu_item_make_label_normal (VnckImageMenuItem *item)
{
  _make_ctk_label_normal (CTK_LABEL (item->accel_label));
}
