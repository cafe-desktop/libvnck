/* vim: set sw=2 et: */
/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VNCK_PAGER_ACCESSIBLE_H__
#define __VNCK_PAGER_ACCESSIBLE_H__

#include <ctk/ctk.h>
#include <atk/atk.h>
#include "pager.h"
#include "screen.h"

G_BEGIN_DECLS

#define VNCK_PAGER_TYPE_ACCESSIBLE                     (vnck_pager_accessible_get_type ())
#define VNCK_PAGER_ACCESSIBLE(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), VNCK_PAGER_TYPE_ACCESSIBLE, VnckPagerAccessible))
#define VNCK_PAGER_ACCESSIBLE_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_PAGER_TYPE_ACCESSIBLE, VnckPagerAccessibleClass))
#define VNCK_PAGER_IS_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VNCK_PAGER_TYPE_ACCESSIBLE))
#define VNCK_PAGER_IS_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), VnckPagerAccessible))
#define VNCK_PAGER_ACCESSIBLE_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_PAGER_TYPE_ACCESSIBLE, VnckPagerAccessibleClass)) 

typedef struct _VnckPagerAccessible VnckPagerAccessible;
typedef struct _VnckPagerAccessibleClass VnckPagerAccessibleClass;

struct _VnckPagerAccessible
{
  CtkAccessible parent;
};

struct _VnckPagerAccessibleClass
{
  CtkAccessibleClass parent_class;
};

GType vnck_pager_accessible_get_type (void) G_GNUC_CONST;

AtkObject* vnck_pager_accessible_new (CtkWidget *widget); 

G_END_DECLS

#endif /* __VNCK_PAGER_ACCESSIBLE_H__ */
