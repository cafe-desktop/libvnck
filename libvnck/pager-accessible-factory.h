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

#ifndef __VNCK_PAGER_ACCESSIBLE_FACTORY_H__
#define __WBCK_PAGER_ACCESSIBLE_FACTORY_H__

#include <atk/atk.h>

G_BEGIN_DECLS

#define VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY               (vnck_pager_accessible_factory_get_type())
#define VNCK_PAGER_ACCESSIBLE_FACTORY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY, VnckpagerAccessibleFactory))
#define VNCK_PAGER_ACCESSIBLE_FACTORY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY, VnckPagerAccessibleFactoryClass))
#define VNCK_IS_PAGER_ACCESSIBLE_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY))
#define VNCK_IS_PAGER_ACCESSIBLE_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY))
#define VNCK_PAGER_ACCESSIBLE_FACTORY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY, VnckPagerAccessibleFactoryClass))

typedef struct _VnckPagerAccessibleFactory       VnckPagerAccessibleFactory;
typedef struct _VnckPagerAccessibleFactoryClass  VnckPagerAccessibleFactoryClass;

struct _VnckPagerAccessibleFactory
{
  AtkObjectFactory parent;
};

struct _VnckPagerAccessibleFactoryClass
{
  AtkObjectFactoryClass parent_class;
};

GType vnck_pager_accessible_factory_get_type (void) G_GNUC_CONST;

AtkObjectFactory* vnck_pager_accessible_factory_new (void);

G_END_DECLS

#endif /* __VNCK_PAGER_ACCESSIBLE_FACTORY_H__ */
