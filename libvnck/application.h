/* application object */
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

#ifndef VNCK_APPLICATION_H
#define VNCK_APPLICATION_H

#include <glib-object.h>
#include <cdk-pixbuf/cdk-pixbuf.h>
#include <libvnck/screen.h>

G_BEGIN_DECLS

#define VNCK_TYPE_APPLICATION              (vnck_application_get_type ())
#define VNCK_APPLICATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_APPLICATION, VnckApplication))
#define VNCK_APPLICATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_APPLICATION, VnckApplicationClass))
#define VNCK_IS_APPLICATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_APPLICATION))
#define VNCK_IS_APPLICATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_APPLICATION))
#define VNCK_APPLICATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_APPLICATION, VnckApplicationClass))

typedef struct _VnckApplicationClass   VnckApplicationClass;
typedef struct _VnckApplicationPrivate VnckApplicationPrivate;

/**
 * VnckApplication:
 *
 * The #VnckApplication struct contains only private fields and should not be
 * directly accessed.
 */
struct _VnckApplication
{
  GObject parent_instance;

  VnckApplicationPrivate *priv;
};

struct _VnckApplicationClass
{
  GObjectClass parent_class;

  /* app name or icon name changed */
  void (* name_changed) (VnckApplication *app);

  /* icon changed */
  void (* icon_changed) (VnckApplication *app);
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

GType vnck_application_get_type (void) G_GNUC_CONST;

VnckApplication* vnck_application_get (gulong xwindow);

gulong vnck_application_get_xid (VnckApplication *app);

GList* vnck_application_get_windows   (VnckApplication *app);
int    vnck_application_get_n_windows (VnckApplication *app);

/* application_get_name, application_get_pid, etc.; prefer to read
 * properties straight off the group leader, and failing that, if the
 * prop is the same for all windows in the app, return the values for
 * the window. Failing that, they make stuff up.
 */
const char* vnck_application_get_name      (VnckApplication *app);
const char* vnck_application_get_icon_name (VnckApplication *app);
int         vnck_application_get_pid       (VnckApplication *app);
CdkPixbuf*  vnck_application_get_icon      (VnckApplication *app);
CdkPixbuf*  vnck_application_get_mini_icon (VnckApplication *app);
gboolean    vnck_application_get_icon_is_fallback (VnckApplication *app);
const char* vnck_application_get_startup_id (VnckApplication *app);

G_END_DECLS

#endif /* VNCK_APPLICATION_H */
