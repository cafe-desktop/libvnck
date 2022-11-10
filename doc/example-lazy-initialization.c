#include <libvnck/libvnck.h>

static void
on_window_opened (VnckScreen *screen,
                  VnckWindow *window,
                  gpointer    data)
{
  /* Note: when this event is emitted while screen is initialized, there is no
   * active window yet. */

  g_print ("%s\n", vnck_window_get_name (window));
}

static void
on_active_window_changed (VnckScreen *screen,
                          VnckWindow *previously_active_window,
                          gpointer    data)
{
  VnckWindow *active_window;

  active_window = vnck_screen_get_active_window (screen);

  if (active_window)
    g_print ("active: %s\n", vnck_window_get_name (active_window));
  else
    g_print ("no active window\n");
}

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;
  VnckScreen *screen;

  gdk_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  screen = vnck_screen_get_default ();

  g_signal_connect (screen, "window-opened",
                    G_CALLBACK (on_window_opened), NULL);
  g_signal_connect (screen, "active-window-changed",
                    G_CALLBACK (on_active_window_changed), NULL);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  return 0;
}
