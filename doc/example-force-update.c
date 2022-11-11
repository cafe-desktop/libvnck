#include <libvnck/libvnck.h>

int
main (int    argc,
      char **argv)
{
  VnckScreen *screen;
  VnckWindow *active_window;
  GList *window_l;

  cdk_init (&argc, &argv);

  screen = vnck_screen_get_default ();

  vnck_screen_force_update (screen);

  active_window = vnck_screen_get_active_window (screen);

  for (window_l = vnck_screen_get_windows (screen); window_l != NULL; window_l = window_l->next)
    {
      VnckWindow *window = VNCK_WINDOW (window_l->data);
      g_print ("%s%s\n", vnck_window_get_name (window),
                         window == active_window ? " (active)" : "");
    }

  return 0;
}
