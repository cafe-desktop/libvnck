/* selector */
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

#if !defined (__LIBVNCK_H_INSIDE__) && !defined (VNCK_COMPILATION)
#error "Only <libvnck/libvnck.h> can be included directly."
#endif

#ifndef VNCK_SELECTOR_H
#define VNCK_SELECTOR_H

#include <ctk/ctk.h>

G_BEGIN_DECLS
#define VNCK_TYPE_SELECTOR              (vnck_selector_get_type ())
#define VNCK_SELECTOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_SELECTOR, VnckSelector))
#define VNCK_SELECTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_SELECTOR, VnckSelectorClass))
#define VNCK_IS_SELECTOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_SELECTOR))
#define VNCK_IS_SELECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_SELECTOR))
#define VNCK_SELECTOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_SELECTOR, VnckSelectorClass))
typedef struct _VnckSelector VnckSelector;
typedef struct _VnckSelectorClass VnckSelectorClass;
typedef struct _VnckSelectorPrivate VnckSelectorPrivate;

/**
 * VnckSelector:
 *
 * The #VnckSelector struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckSelector
{
  CtkMenuBar parent_instance;
  VnckSelectorPrivate *priv;
};

struct _VnckSelectorClass 
{
  CtkMenuBarClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

CtkWidget *vnck_selector_new      (void);
GType      vnck_selector_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* VNCK_SELECTOR_H */
