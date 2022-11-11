/* vim: set sw=2 et: */

#include <libvnck/libvnck.h>
#include <ctk/ctk.h>

static gboolean display_all = FALSE;
static gboolean never_group = FALSE;
static gboolean always_group = FALSE;
static gboolean rtl = FALSE;
static gboolean skip_tasklist = FALSE;
static gboolean transparent = FALSE;
static gboolean vertical = FALSE;
static gint icon_size = VNCK_DEFAULT_MINI_ICON_SIZE;
static gboolean enable_scroll = TRUE;

static GOptionEntry entries[] = {
	{"always-group", 'g', 0, G_OPTION_ARG_NONE, &always_group, "Always group windows", NULL},
	{"never-group", 'n', 0, G_OPTION_ARG_NONE, &never_group, "Never group windows", NULL},
	{"display-all", 'a', 0, G_OPTION_ARG_NONE, &display_all, "Display windows from all workspaces", NULL},
	{"icon-size", 'i', 0, G_OPTION_ARG_INT, &icon_size, "Icon size for tasklist", NULL},
	{"rtl", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Use RTL as default direction", NULL},
	{"skip-tasklist", 's', 0, G_OPTION_ARG_NONE, &skip_tasklist, "Don't show window in tasklist", NULL},
	{"vertical", 'v', 0, G_OPTION_ARG_NONE, &vertical, "Show in vertical mode", NULL},
	{"transparent", 't', 0, G_OPTION_ARG_NONE, &transparent, "Enable Transparency", NULL},
	{"disable-scroll", 'd', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &enable_scroll, "Disable scrolling", NULL},
	{NULL }
};

static gboolean
window_draw (GtkWidget      *widget,
             cairo_t        *cr,
             gpointer        user_data)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1., 1., 1., .5);
  cairo_fill (cr);

  return FALSE;
}

static void
window_composited_changed (GtkWidget *widget,
                           gpointer   user_data)
{
  GdkScreen *screen;
  gboolean composited;

  screen = gdk_screen_get_default ();
  composited = gdk_screen_is_composited (screen);

  ctk_widget_set_app_paintable (widget, composited);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  VnckScreen *screen;
  GtkWidget *win;
  GtkWidget *frame;
  GtkWidget *tasklist;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, ctk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  ctk_init (&argc, &argv);

  if (rtl)
    ctk_widget_set_default_direction (CTK_TEXT_DIR_RTL);

  vnck_set_default_mini_icon_size (icon_size);
  screen = vnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  vnck_screen_force_update (screen);

  win = ctk_window_new (CTK_WINDOW_TOPLEVEL);
  ctk_window_set_default_size (CTK_WINDOW (win), 200, 100);
  ctk_window_stick (CTK_WINDOW (win));
  /*   vnck_ctk_window_set_dock_type (CTK_WINDOW (win)); */

  ctk_window_set_title (CTK_WINDOW (win), "Task List");
  ctk_window_set_resizable (CTK_WINDOW (win), TRUE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (ctk_main_quit),
                    NULL);

  tasklist = vnck_tasklist_new ();

  vnck_tasklist_set_include_all_workspaces (VNCK_TASKLIST (tasklist), display_all);
  if (always_group)
    vnck_tasklist_set_grouping (VNCK_TASKLIST (tasklist),
                                VNCK_TASKLIST_ALWAYS_GROUP);
  else if (never_group)
    vnck_tasklist_set_grouping (VNCK_TASKLIST (tasklist),
                                VNCK_TASKLIST_NEVER_GROUP);
  else
    vnck_tasklist_set_grouping (VNCK_TASKLIST (tasklist),
                                VNCK_TASKLIST_AUTO_GROUP);

  vnck_tasklist_set_scroll_enabled (VNCK_TASKLIST (tasklist), enable_scroll);

  vnck_tasklist_set_middle_click_close (VNCK_TASKLIST (tasklist), TRUE);

  vnck_tasklist_set_orientation (VNCK_TASKLIST (tasklist),
                                 (vertical ? CTK_ORIENTATION_VERTICAL :
                                             CTK_ORIENTATION_HORIZONTAL));

  if (transparent)
    {
      GdkVisual *visual;

      visual = gdk_screen_get_rgba_visual (ctk_widget_get_screen (win));

      if (visual != NULL)
        {
          ctk_widget_set_visual (win, visual);

          g_signal_connect (win, "composited-changed",
                            G_CALLBACK (window_composited_changed),
                            NULL);

          /* draw even if we are not app-painted.
           * this just makes my life a lot easier :)
           */
          g_signal_connect (win, "draw",
                            G_CALLBACK (window_draw),
                            NULL);

          window_composited_changed (win, NULL);
        }

        vnck_tasklist_set_button_relief (VNCK_TASKLIST (tasklist),
                                         CTK_RELIEF_NONE);
    }

  frame = ctk_frame_new (NULL);
  ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_IN);
  ctk_container_add (CTK_CONTAINER (win), frame);

  ctk_container_add (CTK_CONTAINER (frame), tasklist);

  ctk_widget_show (tasklist);
  ctk_widget_show (frame);

  ctk_window_move (CTK_WINDOW (win), 0, 0);

  if (skip_tasklist)
  {
    ctk_window_set_skip_taskbar_hint (CTK_WINDOW (win), TRUE);
    ctk_window_set_keep_above (CTK_WINDOW (win), TRUE);
  }

  ctk_widget_show (win);

  ctk_main ();

  return 0;
}
