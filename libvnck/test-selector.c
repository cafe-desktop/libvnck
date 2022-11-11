/* vim: set sw=2 et: */

#include <libvnck/libvnck.h>
#include <ctk/ctk.h>

static gboolean skip_tasklist = FALSE;

static GOptionEntry entries[] = {
        /* Translators: "tasklist" is the list of running applications (the
         * window list) */
	{"skip-tasklist", 's', 0, G_OPTION_ARG_NONE, &skip_tasklist, "Don't show window in tasklist", NULL},
	{NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  VnckScreen *screen;
  CtkWidget *win;
  CtkWidget *frame;
  CtkWidget *selector;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, ctk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  ctk_init (&argc, &argv);

  screen = vnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  vnck_screen_force_update (screen);

  win = ctk_window_new (CTK_WINDOW_TOPLEVEL);
  ctk_window_set_default_size (CTK_WINDOW (win), 200, 32);
  ctk_window_stick (CTK_WINDOW (win));
  /*   vnck_ctk_window_set_dock_type (CTK_WINDOW (win)); */

  ctk_window_set_title (CTK_WINDOW (win), "Window Selector");
  ctk_window_set_resizable (CTK_WINDOW (win), TRUE);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (ctk_main_quit),
                    NULL);

  selector = vnck_selector_new ();

  frame = ctk_frame_new (NULL);
  ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_IN);
  ctk_container_add (CTK_CONTAINER (win), frame);

  ctk_container_add (CTK_CONTAINER (frame), selector);

  ctk_widget_show (selector);
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
