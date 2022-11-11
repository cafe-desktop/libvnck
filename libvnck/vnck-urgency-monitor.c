/* vim: set sw=2 et: */
/*
 * Copyright (C) 2009 Vincent Untz
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
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <ctk/ctk.h>

#include <libvnck/libvnck.h>

static void
status_icon_activated (GtkStatusIcon *icon,
                       VnckWindow    *window)
{
  VnckWorkspace *workspace;
  guint32 timestamp;

  /* We're in an activate callback, so ctk_get_current_time() works... */
  timestamp = ctk_get_current_event_time ();

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  workspace = vnck_window_get_workspace (window);
  if (workspace)
    vnck_workspace_activate (workspace, timestamp);

  vnck_window_activate (window, timestamp);
}

static GtkStatusIcon *
status_icon_get (VnckWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "vnck-urgency-icon");
}

static void
status_icon_update (VnckWindow *window)
{
  GtkStatusIcon *icon;

  icon = status_icon_get (window);

  if (icon == NULL)
    {
      return;
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (vnck_window_get_icon_is_fallback (window))
    {
      ctk_status_icon_set_from_icon_name (icon, "dialog-information");
    }
  else
    {
      ctk_status_icon_set_from_pixbuf (icon,
                                       vnck_window_get_mini_icon (window));
    }

  ctk_status_icon_set_tooltip_text (icon, vnck_window_get_name (window));
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
status_icon_create (VnckWindow *window)
{
  GtkStatusIcon *icon;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  icon = ctk_status_icon_new ();
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_set_data (G_OBJECT (window), "vnck-urgency-icon", icon);

  g_signal_connect (icon, "activate",
                    G_CALLBACK (status_icon_activated), window);

  status_icon_update (window);
}

static void
status_icon_remove (VnckWindow *window)
{
  GtkStatusIcon *icon;

  icon = status_icon_get (window);
  if (icon != NULL)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      ctk_status_icon_set_visible (icon, FALSE);
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_object_unref (icon);
      g_object_set_data (G_OBJECT (window), "vnck-urgency-icon", NULL);
    }
}

static void
window_state_changed (VnckWindow      *window,
                      VnckWindowState  changed_mask,
                      VnckWindowState  new_state,
                      gpointer         data)
{
  GtkStatusIcon *icon;

  if (!
      (changed_mask &
       (VNCK_WINDOW_STATE_DEMANDS_ATTENTION |
        VNCK_WINDOW_STATE_URGENT)))
    return;

  icon = status_icon_get (window);

  if (vnck_window_or_transient_needs_attention (window))
    {
      if (icon == NULL)
        {
          status_icon_create (window);
        }
    }
  else
    {
      status_icon_remove (window);
    }
}

static void
window_icon_changed (VnckWindow *window,
                     gpointer    data)
{
  status_icon_update (window);
}

static void
window_name_changed (VnckWindow *window,
                     gpointer    data)
{
  status_icon_update (window);
}

static void
connect_to_window (VnckScreen *screen,
                   VnckWindow *window)
{
  if (vnck_window_or_transient_needs_attention (window))
    {
      status_icon_create (window);
    }

  g_signal_connect (window, "state_changed",
                    G_CALLBACK (window_state_changed), NULL);
  g_signal_connect (window, "icon_changed",
                    G_CALLBACK (window_icon_changed), NULL);
  g_signal_connect (window, "name_changed",
                    G_CALLBACK (window_name_changed), NULL);
}

static void
disconnect_from_window (VnckScreen *screen,
                        VnckWindow *window)
{
  status_icon_remove (window);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GError         *error;
  VnckScreen     *screen;

  ctxt = g_option_context_new (NULL);
  g_option_context_set_summary (ctxt, "Monitor windows with the urgency hint "
                                      "set, and display a notification icon "
                                      "for each of them.");
  g_option_context_add_group (ctxt, ctk_get_option_group (TRUE));

  error = NULL;
  if (!g_option_context_parse (ctxt, &argc, &argv, &error))
    {
      g_printerr ("Error while parsing arguments: %s\n", error->message);
      g_option_context_free (ctxt);
      g_error_free (error);
      return 1;
    }

  g_option_context_free (ctxt);
  ctxt = NULL;

  ctk_init (&argc, &argv);

  vnck_set_client_type (VNCK_CLIENT_TYPE_PAGER);

  screen = vnck_screen_get_default ();
  g_signal_connect (screen, "window_opened",
                    G_CALLBACK (connect_to_window), NULL);
  g_signal_connect (screen, "window_closed",
                    G_CALLBACK (disconnect_from_window), NULL);

  ctk_main ();

  return 0;
}
