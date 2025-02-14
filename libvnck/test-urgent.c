#include <ctk/ctk.h>

static void
set_urgent (CtkWidget *window,
            gboolean   urgent)
{
  CtkWidget *label;

  label = ctk_bin_get_child (CTK_BIN (window));

  if (urgent)
    {
      ctk_window_set_urgency_hint (CTK_WINDOW (window), TRUE);
      ctk_window_set_title (CTK_WINDOW (window), "Test Window - Urgent");
      ctk_label_set_text (CTK_LABEL (label), "I am urgent!");
    }
  else
    {
      ctk_window_set_urgency_hint (CTK_WINDOW (window), FALSE);
      ctk_window_set_title (CTK_WINDOW (window), "Test Window");
      ctk_label_set_text (CTK_LABEL (label), "I'm not urgent.");
    }
}

static gboolean
make_urgent (CtkWidget *widget)
{
  set_urgent (widget, TRUE);
  g_object_set_data (G_OBJECT (widget), "vnck-timeout", NULL);

  return FALSE;
}

static gboolean
focused_in (CtkWidget     *widget,
	    CdkEventFocus *event G_GNUC_UNUSED,
	    gpointer       user_data G_GNUC_UNUSED)
{
  guint id;

  set_urgent (widget, FALSE);

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "vnck-timeout"));
  g_object_set_data (G_OBJECT (widget), "vnck-timeout", NULL);

  if (id)
    g_source_remove (id);

  return FALSE;
}

static gboolean
focused_out (CtkWidget     *widget,
	     CdkEventFocus *event G_GNUC_UNUSED,
	     gpointer       user_data G_GNUC_UNUSED)
{
  guint id;

  id = g_timeout_add_seconds (3, (GSourceFunc) make_urgent, widget);
  g_object_set_data (G_OBJECT (widget), "vnck-timeout", GUINT_TO_POINTER (id));

  return FALSE;
}

int
main (int argc, char **argv)
{
  CtkWidget *win;
  CtkWidget *label;

  ctk_init (&argc, &argv);

  win = ctk_window_new (CTK_WINDOW_TOPLEVEL);
  label = ctk_label_new ("");
  ctk_container_add (CTK_CONTAINER (win), label);
  ctk_window_set_keep_above (CTK_WINDOW (win), TRUE);
  ctk_widget_show_all (win);

  g_signal_connect (G_OBJECT (win), "focus-in-event",
                    G_CALLBACK (focused_in),
                    NULL);
  g_signal_connect (G_OBJECT (win), "focus-out-event",
                    G_CALLBACK (focused_out),
                    NULL);
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (ctk_main_quit),
                    NULL);

  set_urgent (win, FALSE);

  ctk_main ();

  return 0;
}
