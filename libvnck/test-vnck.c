#include <libvnck/libvnck.h>
#include <ctk/ctk.h>

static CtkWidget *global_tree_view;
static CtkTreeModel *global_tree_model;
static guint refill_idle;

static void active_window_changed_callback    (VnckScreen      *screen,
                                               VnckWindow      *previous_window,
                                               gpointer         data);
static void active_workspace_changed_callback (VnckScreen      *screen,
                                               VnckWorkspace   *previous_workspace,
                                               gpointer         data);
static void window_stacking_changed_callback  (VnckScreen      *screen,
                                               gpointer         data);
static void window_opened_callback            (VnckScreen      *screen,
                                               VnckWindow      *window,
                                               gpointer         data);
static void window_closed_callback            (VnckScreen      *screen,
                                               VnckWindow      *window,
                                               gpointer         data);
static void workspace_created_callback        (VnckScreen      *screen,
                                               VnckWorkspace   *space,
                                               gpointer         data);
static void workspace_destroyed_callback      (VnckScreen      *screen,
                                               VnckWorkspace   *space,
                                               gpointer         data);
static void application_opened_callback       (VnckScreen      *screen,
                                               VnckApplication *app);
static void application_closed_callback       (VnckScreen      *screen,
                                               VnckApplication *app);
static void showing_desktop_changed_callback  (VnckScreen      *screen,
                                               gpointer         data);
static void window_name_changed_callback      (VnckWindow      *window,
                                               gpointer         data);
static void window_state_changed_callback     (VnckWindow      *window,
                                               VnckWindowState  changed,
                                               VnckWindowState  new,
                                               gpointer         data);
static void window_workspace_changed_callback (VnckWindow      *window,
                                               gpointer         data);
static void window_icon_changed_callback      (VnckWindow      *window,
                                               gpointer         data);
static void window_geometry_changed_callback  (VnckWindow      *window,
                                               gpointer         data);
static void window_class_changed_callback     (VnckWindow      *window,
                                               gpointer         data);
static void window_role_changed_callback      (VnckWindow      *window,
                                               gpointer         data);

static CtkTreeModel* create_tree_model (void);
static CtkWidget*    create_tree_view  (void);
static void          refill_tree_model (CtkTreeModel *model,
                                        VnckScreen   *screen);
static void          update_window     (CtkTreeModel *model,
                                        VnckWindow   *window);
static void          queue_refill_model (void);

static gint icon_size = VNCK_DEFAULT_MINI_ICON_SIZE;

static GOptionEntry entries[] = {
  {"icon-size", 'i', 0, G_OPTION_ARG_INT, &icon_size, "Icon size for tasklist", NULL},
  {NULL }
};


int
main (int argc, char **argv)
{
  VnckScreen *screen;
  CtkWidget *sw;
  CtkWidget *win;
  GOptionContext *ctxt;

  ctxt = g_option_context_new ("");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, ctk_get_option_group (TRUE));
  g_option_context_parse (ctxt, &argc, &argv, NULL);
  g_option_context_free (ctxt);

  vnck_set_default_mini_icon_size (icon_size);

  ctk_init (&argc, &argv);

  screen = vnck_screen_get (0);

  g_signal_connect (G_OBJECT (screen), "active_window_changed",
                    G_CALLBACK (active_window_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
                    G_CALLBACK (active_workspace_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_stacking_changed",
                    G_CALLBACK (window_stacking_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_opened",
                    G_CALLBACK (window_opened_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "window_closed",
                    G_CALLBACK (window_closed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "workspace_created",
                    G_CALLBACK (workspace_created_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "workspace_destroyed",
                    G_CALLBACK (workspace_destroyed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "application_opened",
                    G_CALLBACK (application_opened_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "application_closed",
                    G_CALLBACK (application_closed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (screen), "showing_desktop_changed",
                    G_CALLBACK (showing_desktop_changed_callback),
                    NULL);
  
  global_tree_model = create_tree_model ();
  global_tree_view = create_tree_view ();
  
  ctk_tree_view_set_model (CTK_TREE_VIEW (global_tree_view),
                           global_tree_model);
  
  win = ctk_window_new (CTK_WINDOW_TOPLEVEL);

  ctk_window_set_title (CTK_WINDOW (win), "Window List");
  
  sw = ctk_scrolled_window_new (NULL, NULL);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
                                  CTK_POLICY_AUTOMATIC,
                                  CTK_POLICY_AUTOMATIC);
  
  ctk_container_add (CTK_CONTAINER (sw), global_tree_view);
  ctk_container_add (CTK_CONTAINER (win), sw);

  ctk_window_set_default_size (CTK_WINDOW (win), 650, 550);

  /* quit on window close */
  g_signal_connect (G_OBJECT (win), "destroy",
                    G_CALLBACK (ctk_main_quit),
                    NULL);
  
  ctk_widget_show_all (win);
  
  ctk_main ();
  
  return 0;
}

static void
active_window_changed_callback (VnckScreen *screen,
				VnckWindow *previous_window G_GNUC_UNUSED,
				gpointer    data G_GNUC_UNUSED)
{
  VnckWindow *window;
  
  g_print ("Active window changed\n");

  window = vnck_screen_get_active_window (screen);

  if (window)
    update_window (global_tree_model, window);
}

static void
active_workspace_changed_callback (VnckScreen    *screen G_GNUC_UNUSED,
				   VnckWorkspace *previous_workspace G_GNUC_UNUSED,
				   gpointer       data G_GNUC_UNUSED)
{
  g_print ("Active workspace changed\n");
}

static void
window_stacking_changed_callback (VnckScreen *screen G_GNUC_UNUSED,
				  gpointer    data G_GNUC_UNUSED)
{
  g_print ("Stacking changed\n");
}

static void
window_opened_callback (VnckScreen *screen G_GNUC_UNUSED,
			VnckWindow *window,
			gpointer    data G_GNUC_UNUSED)
{
  g_print ("Window '%s' opened (pid = %d, session_id = %s, role = %s)\n",
           vnck_window_get_name (window),
           vnck_window_get_pid (window),
           vnck_window_get_session_id (window) ?
            vnck_window_get_session_id (window) : "none",
           vnck_window_get_role (window) ?
            vnck_window_get_role (window) : "none");

  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "state_changed",
                    G_CALLBACK (window_state_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "workspace_changed",
                    G_CALLBACK (window_workspace_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "icon_changed",
                    G_CALLBACK (window_icon_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "geometry_changed",
                    G_CALLBACK (window_geometry_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "class_changed",
                    G_CALLBACK (window_class_changed_callback),
                    NULL);
  g_signal_connect (G_OBJECT (window), "role_changed",
                    G_CALLBACK (window_role_changed_callback),
                    NULL);

  queue_refill_model ();
}

static void
window_closed_callback (VnckScreen *screen G_GNUC_UNUSED,
			VnckWindow *window,
			gpointer    data G_GNUC_UNUSED)
{
  g_print ("Window '%s' closed\n",
           vnck_window_get_name (window));

  queue_refill_model ();
}

static void
workspace_created_callback (VnckScreen    *screen G_GNUC_UNUSED,
			    VnckWorkspace *space G_GNUC_UNUSED,
			    gpointer       data G_GNUC_UNUSED)
{
  g_print ("Workspace created\n");
}

static void
workspace_destroyed_callback (VnckScreen    *screen G_GNUC_UNUSED,
			      VnckWorkspace *space G_GNUC_UNUSED,
			      gpointer       data G_GNUC_UNUSED)
{
  g_print ("Workspace destroyed\n");
}

static void
application_opened_callback (VnckScreen      *screen G_GNUC_UNUSED,
			     VnckApplication *app G_GNUC_UNUSED)
{
  g_print ("Application opened\n");
  queue_refill_model ();
}

static void
application_closed_callback (VnckScreen      *screen G_GNUC_UNUSED,
			     VnckApplication *app G_GNUC_UNUSED)
{
  g_print ("Application closed\n");
  queue_refill_model ();
}

static void
showing_desktop_changed_callback (VnckScreen *screen,
				  gpointer    data G_GNUC_UNUSED)
{
  g_print ("Showing desktop now = %d\n",
           vnck_screen_get_showing_desktop (screen));
}

static void
window_name_changed_callback (VnckWindow *window,
			      gpointer    data G_GNUC_UNUSED)
{
  g_print ("Name changed on window '%s'\n",
           vnck_window_get_name (window));

  update_window (global_tree_model, window);
}

static void
window_state_changed_callback (VnckWindow     *window,
			       VnckWindowState changed,
			       VnckWindowState new,
			       gpointer        data G_GNUC_UNUSED)
{
  g_print ("State changed on window '%s'\n",
           vnck_window_get_name (window));

  if (changed & VNCK_WINDOW_STATE_MINIMIZED)
    g_print (" minimized = %d\n", vnck_window_is_minimized (window));

  if (changed & VNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY)
    g_print (" maximized horiz = %d\n", vnck_window_is_maximized_horizontally (window));

  if (changed & VNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)
    g_print (" maximized vert = %d\n", vnck_window_is_maximized_vertically (window));

  if (changed & VNCK_WINDOW_STATE_SHADED)
    g_print (" shaded = %d\n", vnck_window_is_shaded (window));

  if (changed & VNCK_WINDOW_STATE_SKIP_PAGER)
    g_print (" skip pager = %d\n", vnck_window_is_skip_pager (window));

  if (changed & VNCK_WINDOW_STATE_SKIP_TASKLIST)
    g_print (" skip tasklist = %d\n", vnck_window_is_skip_tasklist (window));

  if (changed & VNCK_WINDOW_STATE_STICKY)
    g_print (" sticky = %d\n", vnck_window_is_sticky (window));

  if (changed & VNCK_WINDOW_STATE_FULLSCREEN)
    g_print (" fullscreen = %d\n", vnck_window_is_fullscreen (window));

  g_assert ( ((new & VNCK_WINDOW_STATE_MINIMIZED) != 0) ==
             vnck_window_is_minimized (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY) != 0) ==
             vnck_window_is_maximized_horizontally (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY) != 0) ==
             vnck_window_is_maximized_vertically (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_SHADED) != 0) ==
             vnck_window_is_shaded (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_SKIP_PAGER) != 0) ==
             vnck_window_is_skip_pager (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_SKIP_TASKLIST) != 0) ==
             vnck_window_is_skip_tasklist (window) );
  g_assert ( ((new & VNCK_WINDOW_STATE_STICKY) != 0) ==
             vnck_window_is_sticky (window) );

  update_window (global_tree_model, window);
}

static void
window_workspace_changed_callback (VnckWindow *window,
				   gpointer    data G_GNUC_UNUSED)
{
  VnckWorkspace *space;

  space = vnck_window_get_workspace (window);

  if (space)
    g_print ("Workspace changed on window '%s' to %d\n",
             vnck_window_get_name (window),
             vnck_workspace_get_number (space));
  else
    g_print ("Window '%s' is now pinned to all workspaces\n",
             vnck_window_get_name (window));

  update_window (global_tree_model, window);
}

static void
window_icon_changed_callback (VnckWindow *window,
			      gpointer    data G_GNUC_UNUSED)
{
  g_print ("Icon changed on window '%s'\n",
           vnck_window_get_name (window));

  update_window (global_tree_model, window);
}

static void
window_geometry_changed_callback (VnckWindow *window,
				  gpointer    data G_GNUC_UNUSED)
{
  int x, y, width, height;

  vnck_window_get_geometry (window, &x, &y, &width, &height);

  g_print ("Geometry changed on window '%s': %d,%d  %d x %d\n",
           vnck_window_get_name (window), x, y, width, height);
}

static void
window_class_changed_callback  (VnckWindow *window,
				gpointer    data G_GNUC_UNUSED)
{
  const char *group_name;
  const char *instance_name;

  group_name = vnck_window_get_class_group_name (window);
  instance_name = vnck_window_get_class_instance_name (window);

  g_print ("Class changed on window '%s': %s,%s\n",
           vnck_window_get_name (window), group_name, instance_name);
}

static void
window_role_changed_callback (VnckWindow *window,
			      gpointer    data G_GNUC_UNUSED)
{
  const char *role;

  role = vnck_window_get_role (window);

  g_print ("Role changed on window '%s': %s\n",
           vnck_window_get_name (window), role);
}

static CtkTreeModel*
create_tree_model (void)
{
  CtkListStore *store;
  
  store = ctk_list_store_new (1, VNCK_TYPE_WINDOW);

  return CTK_TREE_MODEL (store);
}

static void
refill_tree_model (CtkTreeModel *model,
                   VnckScreen   *screen)
{
  GList *tmp;
  
  ctk_list_store_clear (CTK_LIST_STORE (model));

  tmp = vnck_screen_get_windows (screen);
  while (tmp != NULL)
    {
      CtkTreeIter iter;
      VnckWindow *window = tmp->data;

      ctk_list_store_append (CTK_LIST_STORE (model), &iter);
      ctk_list_store_set (CTK_LIST_STORE (model), &iter, 0, window, -1);

      if (vnck_window_is_active (window))
        {
          CtkTreeSelection *selection;
          
          selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (global_tree_view));

          ctk_tree_selection_unselect_all (selection);
          
          ctk_tree_selection_select_iter (selection, &iter);
        }
      
      tmp = tmp->next;
    }
  
  ctk_tree_view_columns_autosize (CTK_TREE_VIEW (global_tree_view));
}

static void
update_window (CtkTreeModel *model,
               VnckWindow   *window)
{
  CtkTreeIter iter;
  GList *windows;
  int i;
  
  /* The trick here is to find the right row, we assume
   * the screen and the model are in sync, unless we have a
   * model refill queued, in which case they aren't and we'll update
   * this window in the idle queue anyhow.
   */
  if (refill_idle != 0)
    return;
  
  windows = vnck_screen_get_windows (vnck_window_get_screen (window));

  i = g_list_index (windows, window);

  g_return_if_fail (i >= 0);

  if (ctk_tree_model_iter_nth_child (model, &iter, NULL, i))
    {
      /* Reset the list store value to trigger a recompute */
      ctk_list_store_set (CTK_LIST_STORE (model), &iter, 0, window, -1);

      if (vnck_window_is_active (window))
        {
          CtkTreeSelection *selection;
          
          selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (global_tree_view));

          ctk_tree_selection_unselect_all (selection);

          ctk_tree_selection_select_iter (selection, &iter);
        }
    }
  else
    g_warning ("Tree model has no row %d", i);
}

static VnckWindow*
get_window (CtkTreeModel *model,
            CtkTreeIter  *iter)
{
  VnckWindow *window;
  
  ctk_tree_model_get (model, iter,
                      0, &window,
                      -1);

  /* window may be NULL after we append to the list store and
   * before we set the value with ctk_list_store_set()
   */
  if (window)
    {
      /* we know the model and screen are still holding a reference,
       * so cheat a bit
       */
      g_object_unref (G_OBJECT (window));
    }

  return window;
}

static void
icon_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
	       CtkCellRenderer   *cell,
	       CtkTreeModel      *model,
	       CtkTreeIter       *iter,
	       gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;
  
  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  g_object_set (CTK_CELL_RENDERER (cell),
                "pixbuf", vnck_window_get_mini_icon (window),
                NULL);
}

static void
title_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		CtkCellRenderer   *cell,
		CtkTreeModel      *model,
		CtkTreeIter       *iter,
		gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;

  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  g_object_set (CTK_CELL_RENDERER (cell),
                "text", vnck_window_get_name (window),
                NULL);
}

static void
workspace_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		    CtkCellRenderer   *cell,
		    CtkTreeModel      *model,
		    CtkTreeIter       *iter,
		    gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;
  VnckWorkspace *space;
  char *name;
  
  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  space = vnck_window_get_workspace (window);

  if (space)
    name = g_strdup_printf ("%d", vnck_workspace_get_number (space));
  else if (vnck_window_is_pinned (window))
    name = g_strdup ("all");
  else
    name = g_strdup ("none");
  
  g_object_set (CTK_CELL_RENDERER (cell),
                "text", name,
                NULL);

  g_free (name);
}

static void
pid_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
	      CtkCellRenderer   *cell,
	      CtkTreeModel      *model,
	      CtkTreeIter       *iter,
	      gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;
  int pid;
  char *name;
  
  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  pid = vnck_window_get_pid (window);

  if (pid != 0)
    name = g_strdup_printf ("%d", pid);
  else
    name = g_strdup ("not set");
  
  g_object_set (CTK_CELL_RENDERER (cell),
                "text", name,
                NULL);

  g_free (name);
}

static void
shaded_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		 CtkCellRenderer   *cell,
		 CtkTreeModel      *model,
		 CtkTreeIter       *iter,
		 gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;

  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  ctk_cell_renderer_toggle_set_active (CTK_CELL_RENDERER_TOGGLE (cell),
                                       vnck_window_is_shaded (window));
}

static void
shaded_toggled_callback (CtkCellRendererToggle *cell G_GNUC_UNUSED,
			 char                  *path_string,
			 gpointer               data)
{
  CtkTreeView *tree_view = CTK_TREE_VIEW (data);
  CtkTreeModel *model = ctk_tree_view_get_model (tree_view);
  CtkTreePath *path = ctk_tree_path_new_from_string (path_string);
  CtkTreeIter iter;
  VnckWindow *window;

  ctk_tree_model_get_iter (model, &iter, path);
  window = get_window (model, &iter);

  if (vnck_window_is_shaded (window))
    vnck_window_unshade (window);
  else
    vnck_window_shade (window);
  
  ctk_tree_path_free (path);
}

static void
minimized_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		    CtkCellRenderer   *cell,
		    CtkTreeModel      *model,
		    CtkTreeIter       *iter,
		    gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;

  window = get_window (model, iter);
  if (window == NULL)
    return;

  
  ctk_cell_renderer_toggle_set_active (CTK_CELL_RENDERER_TOGGLE (cell),
                                       vnck_window_is_minimized (window));
}

static void
minimized_toggled_callback (CtkCellRendererToggle *cell G_GNUC_UNUSED,
			    char                  *path_string,
			    gpointer               data)
{
  CtkTreeView *tree_view = CTK_TREE_VIEW (data);
  CtkTreeModel *model = ctk_tree_view_get_model (tree_view);
  CtkTreePath *path = ctk_tree_path_new_from_string (path_string);
  CtkTreeIter iter;
  VnckWindow *window;

  ctk_tree_model_get_iter (model, &iter, path);
  window = get_window (model, &iter);

  if (vnck_window_is_minimized (window))
    /* The toggled callback will only be called in reaction to user
     * button presses or key presses, so ctk_get_current_event_time()
     * should be okay here.
     */
    vnck_window_unminimize (window, ctk_get_current_event_time ());
  else
    vnck_window_minimize (window);
  
  ctk_tree_path_free (path);
}

static void
maximized_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		    CtkCellRenderer   *cell,
		    CtkTreeModel      *model,
		    CtkTreeIter       *iter,
		    gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;

  window = get_window (model, iter);
  if (window == NULL)
    return;

  
  ctk_cell_renderer_toggle_set_active (CTK_CELL_RENDERER_TOGGLE (cell),
                                       vnck_window_is_maximized (window));
}

static void
maximized_toggled_callback (CtkCellRendererToggle *cell G_GNUC_UNUSED,
			    char                  *path_string,
			    gpointer               data)
{
  CtkTreeView *tree_view = CTK_TREE_VIEW (data);
  CtkTreeModel *model = ctk_tree_view_get_model (tree_view);
  CtkTreePath *path = ctk_tree_path_new_from_string (path_string);
  CtkTreeIter iter;
  VnckWindow *window;

  ctk_tree_model_get_iter (model, &iter, path);
  window = get_window (model, &iter);

  if (vnck_window_is_maximized (window))
    vnck_window_unmaximize (window);
  else
    vnck_window_maximize (window);
  
  ctk_tree_path_free (path);
}

static void
session_id_set_func (CtkTreeViewColumn *tree_column G_GNUC_UNUSED,
		     CtkCellRenderer   *cell,
		     CtkTreeModel      *model,
		     CtkTreeIter       *iter,
		     gpointer           data G_GNUC_UNUSED)
{
  VnckWindow *window;
  const char *id;
  
  window = get_window (model, iter);
  if (window == NULL)
    return;
  
  id = vnck_window_get_session_id_utf8 (window);

  g_object_set (CTK_CELL_RENDERER (cell),
                "text", id ? id : "not session managed",
                NULL);
}

static gboolean
selection_func (CtkTreeSelection *selection G_GNUC_UNUSED,
		CtkTreeModel     *model,
		CtkTreePath      *path,
		gboolean          currently_selected,
		gpointer          data G_GNUC_UNUSED)
{
  CtkTreeIter iter;
  VnckWindow *window;
  
  /* Kind of some hack action here. If you try to select a row that's
   * not the active window, we ask the WM to make that window active
   * as a side effect. But we don't actually allow selecting anything
   * that isn't already active. Then, in the active window callback we
   * select the newly-active window
   */
  
  ctk_tree_model_get_iter (model, &iter, path);  

  window = get_window (model, &iter);
  if (window == NULL)
    return FALSE;

  if (currently_selected)
    {
      /* Trying to unselect, not allowed if we are the active window */
      if (vnck_window_is_active (window))
        return FALSE;
      else
        return TRUE;
    }
  else
    {
      if (vnck_window_is_active (window))
        return TRUE;
      else
        {
          /* This should only be called in reaction to user button
           * presses or key presses (I hope), so
           * ctk_get_current_event_time() should be okay here.
           */
          vnck_window_activate (window, ctk_get_current_event_time ());
          return FALSE;
        }
    }
}

static CtkWidget*
create_tree_view (void)
{
  CtkWidget *tree_view;
  CtkCellRenderer *cell_renderer;
  CtkTreeViewColumn *column;
  CtkTreeSelection *selection;
  
  tree_view = ctk_tree_view_new ();

  /* The icon and title are in the same column, so pack
   * two cell renderers into that column
   */
  column = ctk_tree_view_column_new ();
  ctk_tree_view_column_set_title (column, "Window");

  cell_renderer = ctk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (cell_renderer),
                "xpad", 2,
                NULL);
  ctk_tree_view_column_pack_start (column,
                                   cell_renderer,
                                   FALSE);
  ctk_tree_view_column_set_cell_data_func (column, cell_renderer,
                                           icon_set_func, NULL, NULL);
  cell_renderer = ctk_cell_renderer_text_new ();
  ctk_tree_view_column_pack_start (column,
                                   cell_renderer,
                                   TRUE);
  ctk_tree_view_column_set_cell_data_func (column, cell_renderer,
                                           title_set_func, NULL, NULL);

  ctk_tree_view_append_column (CTK_TREE_VIEW (tree_view),
                               column);
  
  /* Then create a workspace column, only one renderer in this column
   * so we get to use insert_column convenience function
   */
  cell_renderer = ctk_cell_renderer_text_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Workspace",
                                              cell_renderer,
                                              workspace_set_func,
                                              NULL,
                                              NULL);

  /* Process ID */
  cell_renderer = ctk_cell_renderer_text_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "PID",
                                              cell_renderer,
                                              pid_set_func,
                                              NULL,
                                              NULL);
  
  /* Shaded checkbox */
  cell_renderer = ctk_cell_renderer_toggle_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Shaded",
                                              cell_renderer,
                                              shaded_set_func,
                                              NULL,
                                              NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "toggled",
                    G_CALLBACK (shaded_toggled_callback),
                    tree_view);

  /* Minimized checkbox */
  cell_renderer = ctk_cell_renderer_toggle_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Minimized",
                                              cell_renderer,
                                              minimized_set_func,
                                              NULL,
                                              NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "toggled",
                    G_CALLBACK (minimized_toggled_callback),
                    tree_view);

  /* Maximized checkbox */
  cell_renderer = ctk_cell_renderer_toggle_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Maximized",
                                              cell_renderer,
                                              maximized_set_func,
                                              NULL,
                                              NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "toggled",
                    G_CALLBACK (maximized_toggled_callback),
                    tree_view);

  /* Session ID */
  cell_renderer = ctk_cell_renderer_text_new ();
  ctk_tree_view_insert_column_with_data_func (CTK_TREE_VIEW (tree_view),
                                              -1, /* append */
                                              "Session ID",
                                              cell_renderer,
                                              session_id_set_func,
                                              NULL,
                                              NULL);
  
  /* The selection will track the active window, so we need to
   * handle it with a custom function
   */
  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (tree_view));
  ctk_tree_selection_set_mode (selection, CTK_SELECTION_MULTIPLE);
  ctk_tree_selection_set_select_function (selection, selection_func, NULL, NULL);
  return tree_view;
}

static gboolean
do_refill_model (gpointer data G_GNUC_UNUSED)
{
  refill_idle = 0;

  refill_tree_model (global_tree_model,
                     vnck_screen_get (0));

  return FALSE;
}

static void
queue_refill_model (void)
{
  if (refill_idle != 0)
    return;

  /* Don't keep any stale references */
  ctk_list_store_clear (CTK_LIST_STORE (global_tree_model));
  
  refill_idle = g_idle_add (do_refill_model, NULL);
}
