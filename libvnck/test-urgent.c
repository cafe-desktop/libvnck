/* vim: set sw=2 et: */

#include <ctk/ctk.h>

static void
set_urgent (GtkWidget *window,
            gboolean   urgent)
{
  GtkWidget *label;

  label = ctk_bin_get_child (GTK_BIN (window));

  if (urgent)
    {
      ctk_window_set_urgency_hint (GTK_WINDOW (window), TRUE);
      ctk_window_set_title (GTK_WINDOW (window), "Test Window - Urgent");
      ctk_label_set_text (GTK_LABEL (label), "I am urgent!");
    }
  else
    {
      ctk_window_set_urgency_hint (GTK_WINDOW (window), FALSE);
      ctk_window_set_title (GTK_WINDOW (window), "Test Window");
      ctk_label_set_text (GTK_LABEL (label), "I'm not urgent.");
    }
}

static gboolean
make_urgent (GtkWidget *widget)
{
  set_urgent (widget, TRUE);
  g_object_set_data (G_OBJECT (widget), "vnck-timeout", NULL);

  return FALSE;
}

static gboolean
focused_in (GtkWidget     *widget,
            GdkEventFocus *event,
            gpointer       user_data)
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
focused_out (GtkWidget     *widget,
             GdkEventFocus *event,
             gpointer       user_data)
{
  guint id;

  id = g_timeout_add_seconds (3, (GSourceFunc) make_urgent, widget);
  g_object_set_data (G_OBJECT (widget), "vnck-timeout", GUINT_TO_POINTER (id));

  return FALSE;
}

int
main (int argc, char **argv)
{
  GtkWidget *win;
  GtkWidget *label;

  ctk_init (&argc, &argv);

  win = ctk_window_new (GTK_WINDOW_TOPLEVEL);
  label = ctk_label_new ("");
  ctk_container_add (GTK_CONTAINER (win), label);
  ctk_window_set_keep_above (GTK_WINDOW (win), TRUE);
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
