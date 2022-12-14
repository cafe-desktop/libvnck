/* vim: set sw=2 et: */

#include <libvnck/libvnck.h>
#include <ctk/ctk.h>

static int n_rows = 1;
static gboolean only_current = FALSE;
static gboolean rtl = FALSE;
static gboolean show_name = FALSE;
static gboolean simple_scrolling = FALSE;
static gboolean vertical = FALSE;
static gboolean wrap_on_scroll = FALSE;

static GOptionEntry entries[] = {
	{"n-rows", 'n', 0, G_OPTION_ARG_INT, &n_rows, "Use N_ROWS rows", "N_ROWS"},
	{"only-current", 'c', 0, G_OPTION_ARG_NONE, &only_current, "Only show current workspace", NULL},
	{"rtl", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Use RTL as default direction", NULL},
	{"show-name", 's', 0, G_OPTION_ARG_NONE, &show_name, "Show workspace names instead of workspace contents", NULL},
	{"simple-scrolling", 'd', 0, G_OPTION_ARG_NONE, &simple_scrolling, "Use the simple 1d scroll mode", NULL},
	{"vertical-orientation", 'v', 0, G_OPTION_ARG_NONE, &vertical, "Use a vertical orientation", NULL},
	{"wrap-on-scroll", 'w', 0, G_OPTION_ARG_NONE, &wrap_on_scroll, "Wrap on scrolling over borders", NULL},
	{NULL }
};

static void
create_pager_window (CtkOrientation       orientation,
                     gboolean             show_all,
                     VnckPagerDisplayMode mode,
                     VnckPagerScrollMode  scroll_mode,
                     int                  rows,
                     gboolean             wrap)
{
  CtkWidget *win;
  CtkWidget *pager;

  win = ctk_window_new (CTK_WINDOW_TOPLEVEL);

  ctk_window_stick (CTK_WINDOW (win));
#if 0
  vnck_ctk_window_set_dock_type (CTK_WINDOW (win));
#endif

  ctk_window_set_title (CTK_WINDOW (win), "Pager");

  /* very very random */
  ctk_window_move (CTK_WINDOW (win), 0, 0);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (ctk_main_quit),
                    NULL);

  pager = vnck_pager_new ();

  vnck_pager_set_show_all (VNCK_PAGER (pager), show_all);
  vnck_pager_set_display_mode (VNCK_PAGER (pager), mode);
  vnck_pager_set_scroll_mode (VNCK_PAGER (pager), scroll_mode);
  vnck_pager_set_orientation (VNCK_PAGER (pager), orientation);
  vnck_pager_set_n_rows (VNCK_PAGER (pager), rows);
  vnck_pager_set_shadow_type (VNCK_PAGER (pager), CTK_SHADOW_IN);
  vnck_pager_set_wrap_on_scroll (VNCK_PAGER (pager), wrap);

  ctk_container_add (CTK_CONTAINER (win), pager);

  ctk_widget_show_all (win);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  CtkOrientation  orientation;
  VnckPagerDisplayMode mode;
  VnckPagerScrollMode scroll_mode;
  VnckScreen *screen;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, ctk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);
  ctxt = NULL;

  ctk_init (&argc, &argv);

  if (rtl)
    ctk_widget_set_default_direction (CTK_TEXT_DIR_RTL);

  screen = vnck_screen_get_default ();

  /* because the pager doesn't respond to signals at the moment */
  vnck_screen_force_update (screen);

  if (vertical)
	  orientation = CTK_ORIENTATION_VERTICAL;
  else
	  orientation = CTK_ORIENTATION_HORIZONTAL;

  if (show_name)
	  mode = VNCK_PAGER_DISPLAY_NAME;
  else
	  mode = VNCK_PAGER_DISPLAY_CONTENT;

  if (simple_scrolling)
	  scroll_mode = VNCK_PAGER_SCROLL_1D;
  else
	  scroll_mode = VNCK_PAGER_SCROLL_2D;

  create_pager_window (orientation, !only_current, mode, scroll_mode, n_rows, wrap_on_scroll);

  ctk_main ();

  return 0;
}
