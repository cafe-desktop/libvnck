#include <libvnck/libvnck.h>

static void
on_active_window_changed (VnckScreen *screen,
			  VnckWindow *previously_active_window G_GNUC_UNUSED,
			  gpointer    data G_GNUC_UNUSED)
{
  VnckWindow *active_window;

  active_window = vnck_screen_get_active_window (screen);

  if (active_window)
    g_print ("active: %s\n", vnck_window_get_name (active_window));
  else
    g_print ("no active window\n");
}

static gboolean
quit_loop (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);

  return FALSE;
}

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;
  VnckScreen *screen;

  cdk_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  while (TRUE)
    {
      screen = vnck_screen_get_default ();

      g_print ("libvnck is active for 5 seconds; change the active window to get notifications\n");
      g_signal_connect (screen, "active-window-changed",
                        G_CALLBACK (on_active_window_changed), NULL);
      g_timeout_add_seconds (5, quit_loop, loop);
      g_main_loop_run (loop);

      g_print ("libvnck is shutting down for 5 seconds; no notification will happen anymore\n");
      vnck_shutdown ();
      g_timeout_add_seconds (5, quit_loop, loop);
      g_main_loop_run (loop);

      g_print ("libvnck is getting reinitialized...\n");
    }

  g_main_loop_unref (loop);

  return 0;
}
