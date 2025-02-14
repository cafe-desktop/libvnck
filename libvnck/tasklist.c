/* tasklist object */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2003, 2005-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include "tasklist.h"
#include "window.h"
#include "class-group.h"
#include "window-action-menu.h"
#include "vnck-image-menu-item-private.h"
#include "workspace.h"
#include "xutils.h"
#include "private.h"

/**
 * SECTION:tasklist
 * @short_description: a tasklist widget, showing the list of windows as a list
 * of buttons.
 * @see_also: #VnckScreen, #VnckSelector
 * @stability: Unstable
 *
 * The #VnckTasklist represents client windows on a screen as a list of buttons
 * labelled with the window titles and icons. Pressing a button can activate or
 * minimize the represented window, and other typical actions are available
 * through a popup menu. Windows needing attention can also be distinguished
 * by a fade effect on the buttons representing them, to help attract the
 * user's attention.
 *
 * The behavior of the #VnckTasklist can be customized in various ways, like
 * grouping multiple windows of the same application in one button (see
 * vnck_tasklist_set_grouping() and vnck_tasklist_set_grouping_limit()), or
 * showing windows from all workspaces (see
 * vnck_tasklist_set_include_all_workspaces()). The fade effect for windows
 * needing attention can be controlled by various style properties like
 * #VnckTasklist:fade-max-loops and #VnckTasklist:fade-opacity.
 *
 * The tasklist also acts as iconification destination. If there are multiple
 * #VnckTasklist or other applications setting the iconification destination
 * for windows, the iconification destinations might not be consistent among
 * windows and it is not possible to determine which #VnckTasklist (or which
 * other application) owns this propriety.
 */

/* TODO:
 *
 *  Add total focused time to the grouping score function
 *  Fine tune the grouping scoring function
 *  Fix "changes" to icon for groups/applications
 *  Maybe fine tune size_allocate() some more...
 *  Better vertical layout handling
 *  prefs
 *  support for right-click menu merging w/ bonobo for the applet
 *
 */


#define VNCK_TYPE_TASK              (vnck_task_get_type ())
#define VNCK_TASK(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), VNCK_TYPE_TASK, VnckTask))
#define VNCK_TASK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), VNCK_TYPE_TASK, VnckTaskClass))
#define VNCK_IS_TASK(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), VNCK_TYPE_TASK))
#define VNCK_IS_TASK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), VNCK_TYPE_TASK))
#define VNCK_TASK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VNCK_TYPE_TASK, VnckTaskClass))

typedef struct _VnckTask        VnckTask;
typedef struct _VnckTaskClass   VnckTaskClass;

#define DEFAULT_GROUPING_LIMIT 80

#define MINI_ICON_SIZE _vnck_get_default_mini_icon_size ()
#define TASKLIST_BUTTON_PADDING 4
#define TASKLIST_TEXT_MAX_WIDTH 25 /* maximum width in characters */

#define N_SCREEN_CONNECTIONS 5

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

typedef enum
{
  VNCK_TASK_CLASS_GROUP,
  VNCK_TASK_WINDOW,
  VNCK_TASK_STARTUP_SEQUENCE
} VnckTaskType;

struct _VnckTask
{
  GObject parent_instance;

  VnckTasklist *tasklist;

  CtkWidget *button;
  CtkWidget *image;
  CtkWidget *label;

  VnckTaskType type;

  VnckClassGroup *class_group;
  VnckWindow *window;
#ifdef HAVE_STARTUP_NOTIFICATION
  SnStartupSequence *startup_sequence;
#endif

  gdouble grouping_score;

  GList *windows; /* List of the VnckTask for the window,
		     if this is a class group */
  guint state_changed_tag;
  guint icon_changed_tag;
  guint name_changed_tag;
  guint class_name_changed_tag;
  guint class_icon_changed_tag;

  /* task menu */
  CtkWidget *menu;
  /* ops menu */
  CtkWidget *action_menu;

  guint really_toggling : 1; /* Set when tasklist really wants
                              * to change the togglebutton state
                              */
  guint was_active : 1;      /* used to fixup activation behavior */

  guint button_activate;

  guint32 dnd_timestamp;

  time_t  start_needs_attention;
  gdouble glow_start_time;
  gdouble glow_factor;

  guint button_glow;

  gint row;
  gint col;
  
  guint resize_idle_id;
};

struct _VnckTaskClass
{
  GObjectClass parent_class;
};

typedef struct _skipped_window
{
  VnckWindow *window;
  gulong tag;
} skipped_window;

struct _VnckTasklistPrivate
{
  VnckScreen *screen;

  VnckTask *active_task; /* NULL if active window not in tasklist */
  VnckTask *active_class_group; /* NULL if active window not in tasklist */

  gboolean include_all_workspaces;

  /* Calculated by update_lists */
  GList *class_groups;
  GList *windows;
  GList *windows_without_class_group;

  /* Not handled by update_lists */
  GList *startup_sequences;

  /* windows with _NET_WM_STATE_SKIP_TASKBAR set; connected to
   * "state_changed" signal, but excluded from tasklist.
   */
  GList *skipped_windows;

  GHashTable *class_group_hash;
  GHashTable *win_hash;

  gint max_button_width;
  gint max_button_height;

  gboolean switch_workspace_on_unminimize;
  gboolean middle_click_close;

  VnckTasklistGroupingType grouping;
  gint grouping_limit;

  guint activate_timeout_id;
  guint screen_connections [N_SCREEN_CONNECTIONS];

  guint idle_callback_tag;

  int *size_hints;
  int size_hints_len;

  VnckLoadIconFunction icon_loader;
  void *icon_loader_data;
  GDestroyNotify free_icon_loader_data;

#ifdef HAVE_STARTUP_NOTIFICATION
  SnMonitorContext *sn_context;
  guint startup_sequence_timeout;
#endif

  CdkMonitor *monitor;
  CdkRectangle monitor_geometry;
  CtkReliefStyle relief;
  CtkOrientation orientation;

  guint drag_start_time;

  gboolean scroll_enabled;
};

static GType vnck_task_get_type (void);

G_DEFINE_TYPE (VnckTask, vnck_task, G_TYPE_OBJECT);
G_DEFINE_TYPE_WITH_PRIVATE (VnckTasklist, vnck_tasklist, CTK_TYPE_CONTAINER);

enum
{
  TASK_ENTER_NOTIFY,
  TASK_LEAVE_NOTIFY,
  LAST_SIGNAL
};

static void vnck_task_finalize    (GObject       *object);

static void vnck_task_stop_glow   (VnckTask *task);

static VnckTask *vnck_task_new_from_window      (VnckTasklist    *tasklist,
						 VnckWindow      *window);
static VnckTask *vnck_task_new_from_class_group (VnckTasklist    *tasklist,
						 VnckClassGroup  *class_group);
#ifdef HAVE_STARTUP_NOTIFICATION
static VnckTask *vnck_task_new_from_startup_sequence (VnckTasklist      *tasklist,
                                                      SnStartupSequence *sequence);
#endif
static gboolean vnck_task_get_needs_attention (VnckTask *task);


static char      *vnck_task_get_text (VnckTask *task,
                                      gboolean  icon_text,
                                      gboolean  include_state);
static GdkPixbuf *vnck_task_get_icon (VnckTask *task);
static gint       vnck_task_compare_alphabetically (gconstpointer  a,
                                                    gconstpointer  b);
static gint       vnck_task_compare  (gconstpointer  a,
				      gconstpointer  b);
static void       vnck_task_update_visible_state (VnckTask *task);
static void       vnck_task_state_changed        (VnckWindow      *window,
                                                  VnckWindowState  changed_mask,
                                                  VnckWindowState  new_state,
                                                  gpointer         data);

static void       vnck_task_drag_begin    (CtkWidget          *widget,
                                           CdkDragContext     *context,
                                           VnckTask           *task);
static void       vnck_task_drag_end      (CtkWidget          *widget,
                                           CdkDragContext     *context,
                                           VnckTask           *task);
static void       vnck_task_drag_data_get (CtkWidget          *widget,
                                           CdkDragContext     *context,
                                           CtkSelectionData   *selection_data,
                                           guint               info,
                                           guint               time,
                                           VnckTask           *task);

static void     vnck_tasklist_finalize      (GObject        *object);

static void     vnck_tasklist_get_preferred_width (CtkWidget *widget,
                                                   int       *minimum_width,
                                                   int       *natural_width);
static void     vnck_tasklist_get_preferred_height (CtkWidget *widget,
                                                    int       *minimum_height,
                                                    int       *natural_height);
static void     vnck_tasklist_size_allocate (CtkWidget        *widget,
                                             CtkAllocation    *allocation);
static void     vnck_tasklist_realize       (CtkWidget        *widget);
static void     vnck_tasklist_unrealize     (CtkWidget        *widget);
static gboolean vnck_tasklist_scroll_event  (CtkWidget        *widget,
                                             CdkEventScroll   *event);
static void     vnck_tasklist_forall        (CtkContainer     *container,
                                             gboolean	       include_internals,
                                             CtkCallback       callback,
                                             gpointer          callback_data);
static void     vnck_tasklist_remove	    (CtkContainer   *container,
					     CtkWidget	    *widget);
static void     vnck_tasklist_free_tasks    (VnckTasklist   *tasklist);
static void     vnck_tasklist_update_lists  (VnckTasklist   *tasklist);
static int      vnck_tasklist_layout        (CtkAllocation  *allocation,
					     int             max_width,
					     int             max_height,
					     int             n_buttons,
                                             CtkOrientation  orientation,
					     int            *n_cols_out,
					     int            *n_rows_out);

static void     vnck_tasklist_active_window_changed    (VnckScreen   *screen,
                                                        VnckWindow   *previous_window,
							VnckTasklist *tasklist);
static void     vnck_tasklist_active_workspace_changed (VnckScreen   *screen,
                                                        VnckWorkspace *previous_workspace,
							VnckTasklist *tasklist);
static void     vnck_tasklist_window_added             (VnckScreen   *screen,
							VnckWindow   *win,
							VnckTasklist *tasklist);
static void     vnck_tasklist_window_removed           (VnckScreen   *screen,
							VnckWindow   *win,
							VnckTasklist *tasklist);
static void     vnck_tasklist_viewports_changed        (VnckScreen   *screen,
							VnckTasklist *tasklist);
static void     vnck_tasklist_connect_window           (VnckTasklist *tasklist,
							VnckWindow   *window);
static void     vnck_tasklist_disconnect_window        (VnckTasklist *tasklist,
							VnckWindow   *window);

static void     vnck_tasklist_change_active_task       (VnckTasklist *tasklist,
							VnckTask *active_task);
static gboolean vnck_tasklist_change_active_timeout    (gpointer data);
static void     vnck_tasklist_activate_task_window     (VnckTask *task,
                                                        guint32   timestamp);

static void     vnck_tasklist_update_icon_geometries   (VnckTasklist *tasklist,
							GList        *visible_tasks);
static void     vnck_tasklist_connect_screen           (VnckTasklist *tasklist);
static void     vnck_tasklist_disconnect_screen        (VnckTasklist *tasklist);

#ifdef HAVE_STARTUP_NOTIFICATION
static void     vnck_tasklist_sn_event                 (SnMonitorEvent *event,
                                                        void           *user_data);
static void     vnck_tasklist_check_end_sequence       (VnckTasklist   *tasklist,
                                                        VnckWindow     *window);
#endif

/*
 * Keep track of all tasklist instances so we can decide
 * whether to show windows from all monitors in the
 * tasklist
 */
static GSList *tasklist_instances;

static void
vnck_task_init (VnckTask *task)
{
  task->type = VNCK_TASK_WINDOW;
  task->resize_idle_id = 0;
}

static void
vnck_task_class_init (VnckTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = vnck_task_finalize;
}

static gboolean
vnck_task_button_glow (VnckTask *task)
{
  gdouble now;
  gfloat fade_opacity, loop_time;
  gint fade_max_loops;
  gboolean stopped;

  now = g_get_real_time () / G_USEC_PER_SEC;

  if (task->glow_start_time <= G_MINDOUBLE)
    task->glow_start_time = now;

  ctk_widget_style_get (CTK_WIDGET (task->tasklist), "fade-opacity", &fade_opacity,
                                                     "fade-loop-time", &loop_time,
                                                     "fade-max-loops", &fade_max_loops,
                                                     NULL);

  if (task->button_glow == 0)
    {
      /* we're in "has stopped glowing" mode */
      task->glow_factor = (gdouble) fade_opacity * 0.5;
      stopped = TRUE;
    }
  else
    {
      task->glow_factor =
        (gdouble) fade_opacity * (0.5 -
                                  0.5 * cos ((now - task->glow_start_time) *
                                             M_PI * 2.0 / (gdouble) loop_time));

      if (now - task->start_needs_attention > (gdouble) loop_time * 1.0 * fade_max_loops)
        stopped = ABS (task->glow_factor - (gdouble) fade_opacity * 0.5) < 0.05;
      else
        stopped = FALSE;
    }

  ctk_widget_queue_draw (task->button);

  if (stopped)
    vnck_task_stop_glow (task);

  return !stopped;
}

static void
vnck_task_clear_glow_start_timeout_id (VnckTask *task)
{
  task->button_glow = 0;
}

static void
vnck_task_queue_glow (VnckTask *task)
{
  if (task->button_glow == 0)
    {
      task->glow_start_time = 0.0;

      /* The animation doesn't speed up or slow down based on the
       * timeout value, but instead will just appear smoother or
       * choppier.
       */
      task->button_glow =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                            50,
                            (GSourceFunc) vnck_task_button_glow, task,
                            (GDestroyNotify) vnck_task_clear_glow_start_timeout_id);
    }
}

static void
vnck_task_stop_glow (VnckTask *task)
{
  /* We stop glowing, but we might still have the task colored,
   * so we don't reset the glow factor */
  if (task->button_glow != 0)
    g_source_remove (task->button_glow);
}

static void
vnck_task_reset_glow (VnckTask *task)
{
  vnck_task_stop_glow (task);
  task->glow_factor = 0.0;
}

static void
vnck_task_finalize (GObject *object)
{
  VnckTask *task;

  task = VNCK_TASK (object);

  if (task->tasklist->priv->active_task == task)
    vnck_tasklist_change_active_task (task->tasklist, NULL);

  if (task->button)
    {
      g_object_remove_weak_pointer (G_OBJECT (task->button),
                                    (void**) &task->button);
      ctk_widget_destroy (task->button);
      task->button = NULL;
      task->image = NULL;
      task->label = NULL;
    }

#ifdef HAVE_STARTUP_NOTIFICATION
  if (task->startup_sequence)
    {
      sn_startup_sequence_unref (task->startup_sequence);
      task->startup_sequence = NULL;
    }
#endif

  g_list_free (task->windows);
  task->windows = NULL;

  if (task->state_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->state_changed_tag);
      task->state_changed_tag = 0;
    }

  if (task->icon_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->icon_changed_tag);
      task->icon_changed_tag = 0;
    }

  if (task->name_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->window,
				   task->name_changed_tag);
      task->name_changed_tag = 0;
    }

  if (task->class_name_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_name_changed_tag);
      task->class_name_changed_tag = 0;
    }

  if (task->class_icon_changed_tag != 0)
    {
      g_signal_handler_disconnect (task->class_group,
				   task->class_icon_changed_tag);
      task->class_icon_changed_tag = 0;
    }

  if (task->class_group)
    {
      g_object_unref (task->class_group);
      task->class_group = NULL;
    }

  if (task->window)
    {
      g_object_unref (task->window);
      task->window = NULL;
    }

  if (task->menu)
    {
      ctk_widget_destroy (task->menu);
      task->menu = NULL;
    }

  if (task->action_menu)
    {
      g_object_remove_weak_pointer (G_OBJECT (task->action_menu),
                                    (void**) &task->action_menu);
      ctk_widget_destroy (task->action_menu);
      task->action_menu = NULL;
    }

  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
    }

  vnck_task_stop_glow (task);

  if (task->resize_idle_id > 0)
    {
      g_source_remove (task->resize_idle_id);
      task->resize_idle_id = 0;
    }

  G_OBJECT_CLASS (vnck_task_parent_class)->finalize (object);
}

static guint signals[LAST_SIGNAL] = { 0 };

static void
vnck_tasklist_init (VnckTasklist *tasklist)
{
  CtkWidget *widget;
  AtkObject *atk_obj;

  widget = CTK_WIDGET (tasklist);

  ctk_widget_set_has_window (widget, FALSE);

  tasklist->priv = vnck_tasklist_get_instance_private (tasklist);

  tasklist->priv->class_group_hash = g_hash_table_new (NULL, NULL);
  tasklist->priv->win_hash = g_hash_table_new (NULL, NULL);

  tasklist->priv->grouping = VNCK_TASKLIST_AUTO_GROUP;
  tasklist->priv->grouping_limit = DEFAULT_GROUPING_LIMIT;

  tasklist->priv->monitor = NULL;
  tasklist->priv->monitor_geometry.width = -1; /* invalid value */
  tasklist->priv->relief = CTK_RELIEF_NORMAL;
  tasklist->priv->orientation = CTK_ORIENTATION_HORIZONTAL;
  tasklist->priv->scroll_enabled = TRUE;

  atk_obj = ctk_widget_get_accessible (widget);
  atk_object_set_name (atk_obj, _("Window List"));
  atk_object_set_description (atk_obj, _("Tool to switch between visible windows"));

#if 0
  /* This doesn't work because, and I think this is because we have no window;
   * therefore, we use the scroll events on task buttons instead */
  ctk_widget_add_events (widget, CDK_SCROLL_MASK);
#endif
}

static void
vnck_tasklist_class_init (VnckTasklistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);
  CtkContainerClass *container_class = CTK_CONTAINER_CLASS (klass);

  object_class->finalize = vnck_tasklist_finalize;

  widget_class->get_preferred_width = vnck_tasklist_get_preferred_width;
  widget_class->get_preferred_height = vnck_tasklist_get_preferred_height;
  widget_class->size_allocate = vnck_tasklist_size_allocate;
  widget_class->realize = vnck_tasklist_realize;
  widget_class->unrealize = vnck_tasklist_unrealize;
#if 0
  /* See comment above ctk_widget_add_events() in vnck_tasklist_init() */
  widget_class->scroll_event = vnck_tasklist_scroll_event;
#endif

  container_class->forall = vnck_tasklist_forall;
  container_class->remove = vnck_tasklist_remove;

  /**
   * VnckTasklist:fade-loop-time:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the time one loop of this
   * fade effect takes, in seconds.
   *
   * Since: 2.16
   */
  ctk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-loop-time",
                                                              "Loop time",
                                                              "The time one loop takes when fading, in seconds. Default: 3.0",
                                                              0.2, 10.0, 3.0,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * VnckTasklist:fade-max-loops:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the number of loops for
   * this fade effect. 0 means the button will only fade to the final color.
   *
   * Since: 2.20
   */
  ctk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("fade-max-loops",
                                                              "Maximum number of loops",
                                                              "The number of fading loops. 0 means the button will only fade to the final color. Default: 5",
                                                              0, 50, 5,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * VnckTasklist:fade-overlay-rect:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. Set this property to %TRUE to enable a
   * compatibility mode for pixbuf engine themes that cannot react to color
   * changes. If enabled, a rectangle with the correct color will be drawn on
   * top of the button.
   *
   * Since: 2.16
   */
  ctk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("fade-overlay-rect",
                                                                 "Overlay a rectangle, instead of modifying the background.",
                                                                 "Compatibility mode for pixbuf engine themes that cannot react to color changes. If enabled, a rectangle with the correct color will be drawn on top of the button. Default: TRUE",
                                                                 TRUE,
                                                                 G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * VnckTasklist:fade-opacity:
   *
   * When a window needs attention, a fade effect is drawn on the button
   * representing the window. This property controls the final opacity that
   * will be reached by the fade effect.
   *
   * Since: 2.16
   */
  ctk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("fade-opacity",
                                                              "Final opacity",
                                                              "The final opacity that will be reached. Default: 0.8",
                                                              0.0, 1.0, 0.8,
                                                              G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  ctk_widget_class_set_css_name (widget_class, "vnck-tasklist");

  /**
   * VnckTasklist::task-enter-notify:
   * @tasklist: the #VnckTasklist which emitted the signal.
   * @windows: the #GList with all the #VnckWindow belonging to the task.
   *
   * Emitted when the task is entered.
   */
  signals[TASK_ENTER_NOTIFY] =
    g_signal_new ("task_enter_notify",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  /**
   * VnckTasklist::task-leave-notify:
   * @tasklist: the #VnckTasklist which emitted the signal.
   * @windows: the #GList with all the #VnckWindow belonging to the task.
   *
   * Emitted when the task is entered.
   */
  signals[TASK_LEAVE_NOTIFY] =
    g_signal_new ("task_leave_notify",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
vnck_tasklist_free_skipped_windows (VnckTasklist  *tasklist)
{
  GList *l;

  l = tasklist->priv->skipped_windows;

  while (l != NULL)
    {
      skipped_window *skipped = (skipped_window*) l->data;
      g_signal_handler_disconnect (skipped->window, skipped->tag);
      g_object_unref (skipped->window);
      g_free (skipped);
      l = l->next;
    }

  g_list_free (tasklist->priv->skipped_windows);
}

static void
vnck_tasklist_finalize (GObject *object)
{
  VnckTasklist *tasklist;

  tasklist = VNCK_TASKLIST (object);

  /* Tasks should have gone away due to removing their
   * buttons in container destruction
   */
  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (tasklist->priv->windows == NULL);
  g_assert (tasklist->priv->windows_without_class_group == NULL);
  g_assert (tasklist->priv->startup_sequences == NULL);
  /* vnck_tasklist_free_tasks (tasklist); */

  if (tasklist->priv->skipped_windows)
    {
      vnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }

  g_hash_table_destroy (tasklist->priv->class_group_hash);
  tasklist->priv->class_group_hash = NULL;

  g_hash_table_destroy (tasklist->priv->win_hash);
  tasklist->priv->win_hash = NULL;

  if (tasklist->priv->activate_timeout_id != 0)
    {
      g_source_remove (tasklist->priv->activate_timeout_id);
      tasklist->priv->activate_timeout_id = 0;
    }

  if (tasklist->priv->idle_callback_tag != 0)
    {
      g_source_remove (tasklist->priv->idle_callback_tag);
      tasklist->priv->idle_callback_tag = 0;
    }

  g_free (tasklist->priv->size_hints);
  tasklist->priv->size_hints = NULL;
  tasklist->priv->size_hints_len = 0;

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);
  tasklist->priv->free_icon_loader_data = NULL;
  tasklist->priv->icon_loader_data = NULL;

  G_OBJECT_CLASS (vnck_tasklist_parent_class)->finalize (object);
}

/**
 * vnck_tasklist_set_grouping:
 * @tasklist: a #VnckTasklist.
 * @grouping: a grouping policy.
 *
 * Sets the grouping policy for @tasklist to @grouping.
 */
void
vnck_tasklist_set_grouping (VnckTasklist            *tasklist,
			    VnckTasklistGroupingType grouping)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping == grouping)
    return;

  tasklist->priv->grouping = grouping;
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static void
vnck_tasklist_set_relief_callback (VnckWindow   *win G_GNUC_UNUSED,
				   VnckTask     *task,
				   VnckTasklist *tasklist)
{
  ctk_button_set_relief (CTK_BUTTON (task->button), tasklist->priv->relief);
}

/**
 * vnck_tasklist_set_button_relief:
 * @tasklist: a #VnckTasklist.
 * @relief: a relief type.
 *
 * Sets the relief type of the buttons in @tasklist to @relief. The main use of
 * this function is proper integration of #VnckTasklist in panels with
 * non-system backgrounds.
 *
 * Since: 2.12
 */
void
vnck_tasklist_set_button_relief (VnckTasklist *tasklist, CtkReliefStyle relief)
{
  GList *walk;

  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  if (relief == tasklist->priv->relief)
    return;

  tasklist->priv->relief = relief;

  g_hash_table_foreach (tasklist->priv->win_hash,
                        (GHFunc) vnck_tasklist_set_relief_callback,
                        tasklist);
  for (walk = tasklist->priv->class_groups; walk; walk = g_list_next (walk))
    ctk_button_set_relief (CTK_BUTTON (VNCK_TASK (walk->data)->button), relief);
}

/**
 * vnck_tasklist_set_middle_click_close:
 * @tasklist: a #VnckTasklist.
 * @middle_click_close: whether to close windows with middle click on
 * button.
 *
 * Sets @tasklist to close windows with mouse middle click on button,
 * according to @middle_click_close.
 *
 * Since: 3.4.6
 */
void
vnck_tasklist_set_middle_click_close (VnckTasklist  *tasklist,
				      gboolean       middle_click_close)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  tasklist->priv->middle_click_close = middle_click_close;
}

/**
 * vnck_tasklist_set_orientation:
 * @tasklist: a #VnckTasklist.
 * @orient: a CtkOrientation.
 *
 * Set the orientation of the @tasklist to match @orient.
 * This function can be used to integrate a #VnckTasklist in vertical panels.
 *
 * Since: 3.4.6
 */
void vnck_tasklist_set_orientation (VnckTasklist *tasklist,
				    CtkOrientation orient)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  tasklist->priv->orientation = orient;
}

/**
 * vnck_tasklist_set_scroll_enabled:
 * @tasklist: a #VnckTasklist.
 * @scroll_enabled: a boolean.
 *
 * Sets the scroll behavior of the @tasklist. When set to %TRUE, a scroll
 * event over the tasklist will change the current window accordingly.
 *
 * Since: 3.24.0
 */
void
vnck_tasklist_set_scroll_enabled (VnckTasklist *tasklist,
                                  gboolean      scroll_enabled)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  tasklist->priv->scroll_enabled = scroll_enabled;
}

/**
 * vnck_tasklist_get_scroll_enabled:
 * @tasklist: a #VnckTasklist.
 *
 * Gets the scroll behavior of the @tasklist.
 *
 * Since: 3.24.0
 */
gboolean
vnck_tasklist_get_scroll_enabled (VnckTasklist *tasklist)
{
  g_return_val_if_fail (VNCK_IS_TASKLIST (tasklist), TRUE);

  return tasklist->priv->scroll_enabled;
}

/**
 * vnck_tasklist_set_switch_workspace_on_unminimize:
 * @tasklist: a #VnckTasklist.
 * @switch_workspace_on_unminimize: whether to activate the #VnckWorkspace a
 * #VnckWindow is on when unminimizing it.
 *
 * Sets @tasklist to activate or not the #VnckWorkspace a #VnckWindow is on
 * when unminimizing it, according to @switch_workspace_on_unminimize.
 *
 * FIXME: does it still work?
 */
void
vnck_tasklist_set_switch_workspace_on_unminimize (VnckTasklist  *tasklist,
						  gboolean       switch_workspace_on_unminimize)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  tasklist->priv->switch_workspace_on_unminimize = switch_workspace_on_unminimize;
}

/**
 * vnck_tasklist_set_include_all_workspaces:
 * @tasklist: a #VnckTasklist.
 * @include_all_workspaces: whether to display #VnckWindow from all
 * #VnckWorkspace in @tasklist.
 *
 * Sets @tasklist to display #VnckWindow from all #VnckWorkspace or not,
 * according to @include_all_workspaces.
 *
 * Note that if the active #VnckWorkspace has a viewport and if
 * @include_all_workspaces is %FALSE, then only the #VnckWindow visible in the
 * viewport are displayed in @tasklist. The rationale for this is that the
 * viewport is generally used to implement workspace-like behavior. A
 * side-effect of this is that, when using multiple #VnckWorkspace with
 * viewport, it is not possible to show all #VnckWindow from a #VnckWorkspace
 * (even those that are not visible in the viewport) in @tasklist without
 * showing all #VnckWindow from all #VnckWorkspace.
 */
void
vnck_tasklist_set_include_all_workspaces (VnckTasklist *tasklist,
					  gboolean      include_all_workspaces)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  include_all_workspaces = (include_all_workspaces != 0);

  if (tasklist->priv->include_all_workspaces == include_all_workspaces)
    return;

  tasklist->priv->include_all_workspaces = include_all_workspaces;
  vnck_tasklist_update_lists (tasklist);
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

/**
 * vnck_tasklist_set_grouping_limit:
 * @tasklist: a #VnckTasklist.
 * @limit: a size in pixels.
 *
 * Sets the maximum size of buttons in @tasklist before @tasklist tries to
 * group #VnckWindow in the same #VnckApplication in only one button. This
 * limit is valid only when the grouping policy of @tasklist is
 * %VNCK_TASKLIST_AUTO_GROUP.
 */
void
vnck_tasklist_set_grouping_limit (VnckTasklist *tasklist,
				  gint          limit)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->grouping_limit == limit)
    return;

  tasklist->priv->grouping_limit = limit;
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

/**
 * vnck_tasklist_set_icon_loader:
 * @tasklist: a #VnckTasklist
 * @load_icon_func: icon loader function
 * @data: data for icon loader function
 * @free_data_func: function to free the data
 *
 * Sets a function to be used for loading icons.
 *
 * Since: 2.2
 **/
void
vnck_tasklist_set_icon_loader (VnckTasklist         *tasklist,
                               VnckLoadIconFunction  load_icon_func,
                               void                 *data,
                               GDestroyNotify        free_data_func)
{
  g_return_if_fail (VNCK_IS_TASKLIST (tasklist));

  if (tasklist->priv->free_icon_loader_data != NULL)
    (* tasklist->priv->free_icon_loader_data) (tasklist->priv->icon_loader_data);

  tasklist->priv->icon_loader = load_icon_func;
  tasklist->priv->icon_loader_data = data;
  tasklist->priv->free_icon_loader_data = free_data_func;
}

/* returns the maximal possible button width (i.e. if you
 * don't want to stretch the buttons to fill the alloctions
 * the width can be smaller) */
static int
vnck_tasklist_layout (CtkAllocation *allocation,
		      int            max_width,
		      int            max_height,
		      int            n_buttons,
		      CtkOrientation orientation,
		      int           *n_cols_out,
		      int           *n_rows_out)
{
  int n_cols, n_rows;

  if (n_buttons == 0)
    {
      *n_cols_out = 0;
      *n_rows_out = 0;
      return 0;
    }

  if (orientation == CTK_ORIENTATION_HORIZONTAL)
    {
      /* How many rows fit in the allocation */
      n_rows = allocation->height / max_height;

      /* Don't have more rows than buttons */
      n_rows = MIN (n_rows, n_buttons);

      /* At least one row */
      n_rows = MAX (n_rows, 1);

      /* We want to use as many cols as possible to limit the width */
      n_cols = (n_buttons + n_rows - 1) / n_rows;

      /* At least one column */
      n_cols = MAX (n_cols, 1);
    }
  else
    {
      /* How many cols fit in the allocation */
      n_cols = allocation->width / max_width;

      /* Don't have more cols than buttons */
      n_cols = MIN (n_cols, n_buttons);

      /* At least one col */
      n_cols = MAX (n_cols, 1);

      /* We want to use as many rows as possible to limit the height */
      n_rows = (n_buttons + n_cols - 1) / n_cols;

      /* At least one row */
      n_rows = MAX (n_rows, 1);
    }

  *n_cols_out = n_cols;
  *n_rows_out = n_rows;

  return allocation->width / n_cols;
}

static void
vnck_tasklist_score_groups (VnckTasklist *tasklist G_GNUC_UNUSED,
			    GList        *ungrouped_class_groups)
{
  VnckTask *class_group_task;
  VnckTask *win_task;
  GList *l, *w;
  const char *first_name = NULL;
  int n_windows;
  int n_same_title;
  double same_window_ratio;

  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = VNCK_TASK (l->data);

      n_windows = g_list_length (class_group_task->windows);

      n_same_title = 0;
      w = class_group_task->windows;
      while (w != NULL)
	{
	  win_task = VNCK_TASK (w->data);

	  if (first_name == NULL)
	    {
              if (vnck_window_has_icon_name (win_task->window))
                first_name = vnck_window_get_icon_name (win_task->window);
              else
                first_name = vnck_window_get_name (win_task->window);
	      n_same_title++;
	    }
	  else
	    {
              const char *name;

              if (vnck_window_has_icon_name (win_task->window))
                name = vnck_window_get_icon_name (win_task->window);
              else
                name = vnck_window_get_name (win_task->window);

	      if (strcmp (name, first_name) == 0)
		n_same_title++;
	    }

	  w = w->next;
	}
      same_window_ratio = (double)n_same_title/(double)n_windows;

      /* FIXME: This is fairly bogus and should be researched more.
       *        XP groups by least used, so we probably want to add
       *        total focused time to this expression.
       */
      class_group_task->grouping_score = -same_window_ratio * 5 + n_windows;

      l = l->next;
    }
}

static GList *
vnck_task_get_highest_scored (GList     *ungrouped_class_groups,
			      VnckTask **class_group_task_out)
{
  VnckTask *class_group_task;
  VnckTask *best_task = NULL;
  double max_score = -1000000000.0; /* Large negative score */
  GList *l;

  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = VNCK_TASK (l->data);

      if (class_group_task->grouping_score >= max_score)
	{
	  max_score = class_group_task->grouping_score;
	  best_task = class_group_task;
	}

      l = l->next;
    }

  *class_group_task_out = best_task;

  return g_list_remove (ungrouped_class_groups, best_task);
}

static int
vnck_tasklist_get_button_size (CtkWidget *widget)
{
  CtkStyleContext *style_context;
  CtkStateFlags state;
  PangoContext *context;
  PangoFontMetrics *metrics;
  PangoFontDescription *description;
  gint char_width;
  gint text_width;
  gint width;

  style_context = ctk_widget_get_style_context (widget);
  state = ctk_style_context_get_state (style_context);
  ctk_style_context_get (style_context, state, CTK_STYLE_PROPERTY_FONT, &description, NULL);

  context = ctk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, description,
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);
  text_width = PANGO_PIXELS (TASKLIST_TEXT_MAX_WIDTH * char_width);

  width = text_width + 2 * TASKLIST_BUTTON_PADDING
	  + MINI_ICON_SIZE + 2 * TASKLIST_BUTTON_PADDING;

  return width;
}

static void
vnck_tasklist_size_request  (CtkWidget      *widget,
                             CtkRequisition *requisition)
{
  VnckTasklist *tasklist;
  CtkRequisition child_req;
  CtkAllocation  tasklist_allocation;
  CtkAllocation  fake_allocation;
  int max_height = 1;
  int max_width = 1;
  /* int u_width, u_height; */
  GList *l;
  GArray *array;
  GList *ungrouped_class_groups;
  int n_windows;
  int n_startup_sequences;
  int n_rows;
  int n_cols, last_n_cols;
  int n_grouped_buttons;
  gboolean score_set;
  int val;
  VnckTask *class_group_task;
  int lowest_range;
  int grouping_limit;

  tasklist = VNCK_TASKLIST (widget);

  /* Calculate max needed height and width of the buttons */
#define GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS(list)                 \
  l = list;                                                     \
  while (l != NULL)                                             \
    {                                                           \
      VnckTask *task = VNCK_TASK (l->data);                     \
                                                                \
      ctk_widget_get_preferred_size (task->button,              \
                                     &child_req, NULL);         \
                                                                \
      max_height = MAX (child_req.height,                       \
			max_height);                            \
      max_width = MAX (child_req.width,                         \
		       max_width);                              \
                                                                \
      l = l->next;                                              \
    }

  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->windows)
  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->class_groups)
  GET_MAX_WIDTH_HEIGHT_FROM_BUTTONS (tasklist->priv->startup_sequences)

  /* Note that the fact that we nearly don't care about the width/height
   * requested by the buttons makes it possible to hide/show the label/image
   * in vnck_task_size_allocated(). If we really cared about those, this
   * wouldn't work since our call to ctk_widget_size_request() does not take
   * into account the hidden widgets.
   */
  tasklist->priv->max_button_width = vnck_tasklist_get_button_size (widget);
  tasklist->priv->max_button_height = max_height;

  ctk_widget_get_allocation (CTK_WIDGET (tasklist), &tasklist_allocation);

  fake_allocation.width = tasklist_allocation.width;
  fake_allocation.height = tasklist_allocation.height;

  array = g_array_new (FALSE, FALSE, sizeof (int));

  /* Calculate size_hints list */

  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);

  /* Try ungrouped mode */
  vnck_tasklist_layout (&fake_allocation,
			tasklist->priv->max_button_width,
			tasklist->priv->max_button_height,
			n_windows + n_startup_sequences,
			tasklist->priv->orientation,
			&n_cols, &n_rows);

  last_n_cols = G_MAXINT;
  lowest_range = G_MAXINT;
  if (tasklist->priv->grouping != VNCK_TASKLIST_ALWAYS_GROUP)
    {
      if (tasklist->priv->orientation == CTK_ORIENTATION_HORIZONTAL)
        {
          val = n_cols * tasklist->priv->max_button_width;
          g_array_insert_val (array, array->len, val);
          val = n_cols * grouping_limit;
          g_array_insert_val (array, array->len, val);

          last_n_cols = n_cols;
          lowest_range = val;
        }
      else
        {
          val = n_rows * tasklist->priv->max_button_height;
          g_array_insert_val (array, array->len, val);
          val = n_rows * grouping_limit;
          g_array_insert_val (array, array->len, val);

          last_n_cols = n_rows;
          lowest_range = val;
        }
    }

  while (ungrouped_class_groups != NULL &&
	 tasklist->priv->grouping != VNCK_TASKLIST_NEVER_GROUP)
    {
      if (!score_set)
        {
          vnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
          score_set = TRUE;
        }

      ungrouped_class_groups = vnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      vnck_tasklist_layout (&fake_allocation,
			    tasklist->priv->max_button_width,
			    tasklist->priv->max_button_height,
			    n_startup_sequences + n_windows - n_grouped_buttons,
			    tasklist->priv->orientation,
			    &n_cols, &n_rows);

      if (tasklist->priv->orientation == CTK_ORIENTATION_HORIZONTAL)
        {
          if (n_cols != last_n_cols &&
              (tasklist->priv->grouping == VNCK_TASKLIST_AUTO_GROUP ||
               ungrouped_class_groups == NULL))
            {
              val = n_cols * tasklist->priv->max_button_width;
              if (val >= lowest_range)
                {
                  /* Overlaps old range */
                  g_assert (array->len > 0);
                  lowest_range = n_cols * grouping_limit;
                  g_array_index(array, int, array->len-1) = lowest_range;
                }
              else
                {
                  /* Full new range */
                  g_array_insert_val (array, array->len, val);
                  val = n_cols * grouping_limit;
                  g_array_insert_val (array, array->len, val);
                  lowest_range = val;
                }

              last_n_cols = n_cols;
            }
        }
      else
        {
          if (n_rows != last_n_cols &&
              (tasklist->priv->grouping == VNCK_TASKLIST_AUTO_GROUP ||
               ungrouped_class_groups == NULL))
            {
              val = n_rows * tasklist->priv->max_button_height;
              if (val >= lowest_range)
                {
                  /* Overlaps old range */
                  g_assert (array->len > 0);
                  lowest_range = n_rows * grouping_limit;
                  g_array_index (array, int, array->len-1) = lowest_range;
                }
              else
                {
                  /* Full new range */
                  g_array_insert_val (array, array->len, val);
                  val = n_rows * grouping_limit;
                  g_array_insert_val (array, array->len, val);
                  lowest_range = val;
                }

              last_n_cols = n_rows;
            }
        }
    }

  g_list_free (ungrouped_class_groups);

  /* Always let you go down to a zero size: */
  if (array->len > 0)
    {
      g_array_index(array, int, array->len-1) = 0;
    }
  else
    {
      val = 0;
      g_array_insert_val (array, 0, val);
      g_array_insert_val (array, 0, val);
    }

  if (tasklist->priv->size_hints)
    g_free (tasklist->priv->size_hints);

  tasklist->priv->size_hints_len = array->len;
  tasklist->priv->size_hints = (int *)g_array_free (array, FALSE);

  if (tasklist->priv->orientation == CTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width = tasklist->priv->size_hints[0];
      requisition->height = fake_allocation.height;
    }
  else
    {
      requisition->width = fake_allocation.width;
      requisition->height = tasklist->priv->size_hints[0];
    }
}

static void
vnck_tasklist_get_preferred_width (CtkWidget *widget,
                                   int       *minimum_width,
                                   int       *natural_width)
{
  CtkRequisition req;

  vnck_tasklist_size_request (widget, &req);

  *minimum_width = *natural_width = req.width;
}

static void
vnck_tasklist_get_preferred_height (CtkWidget *widget,
                                   int       *minimum_height,
                                   int       *natural_height)
{
  CtkRequisition req;

  vnck_tasklist_size_request (widget, &req);

  *minimum_height = *natural_height = req.height;
}


/**
 * vnck_tasklist_get_size_hint_list:
 * @tasklist: a #VnckTasklist.
 * @n_elements: return location for the number of elements in the array
 * returned by this function. This number should always be pair.
 *
 * Since a #VnckTasklist does not have a fixed size (#VnckWindow can be grouped
 * when needed, for example), the standard size request mechanism in CTK+ is
 * not enough to announce what sizes can be used by @tasklist. The size hints
 * mechanism is a solution for this. See panel_applet_set_size_hints() for more
 * information.
 *
 * Return value: a list of size hints that can be used to allocate an
 * appropriate size for @tasklist.
 */
const int *
vnck_tasklist_get_size_hint_list (VnckTasklist  *tasklist,
				  int           *n_elements)
{
  g_return_val_if_fail (VNCK_IS_TASKLIST (tasklist), NULL);
  g_return_val_if_fail (n_elements != NULL, NULL);

  *n_elements = tasklist->priv->size_hints_len;
  return tasklist->priv->size_hints;
}

static gboolean
task_button_queue_resize (gpointer user_data)
{
  VnckTask *task = VNCK_TASK (user_data);

  ctk_widget_queue_resize (task->button);
  task->resize_idle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
vnck_task_size_allocated (CtkWidget     *widget,
                          CtkAllocation *allocation,
                          gpointer       data)
{
  VnckTask        *task = VNCK_TASK (data);
  CtkStyleContext *context;
  CtkStateFlags    state;
  CtkBorder        padding;
  int              min_image_width;
  gboolean         old_image_visible;
  gboolean         old_label_visible;

  context = ctk_widget_get_style_context (widget);
  state = ctk_style_context_get_state (context);
  ctk_style_context_get_padding (context, state, &padding);

  min_image_width = MINI_ICON_SIZE +
                    padding.left + padding.right +
                    2 * TASKLIST_BUTTON_PADDING;
  old_image_visible = ctk_widget_get_visible (task->image);
  old_label_visible = ctk_widget_get_visible (task->label);

  if ((allocation->width < min_image_width + 2 * TASKLIST_BUTTON_PADDING) &&
      (allocation->width >= min_image_width)) {
    ctk_widget_show (task->image);
    ctk_widget_hide (task->label);
  } else if (allocation->width < min_image_width) {
    ctk_widget_hide (task->image);
    ctk_widget_show (task->label);
  } else {
    ctk_widget_show (task->image);
    ctk_widget_show (task->label);
  }

  if (old_image_visible != ctk_widget_get_visible (task->image) ||
      old_label_visible != ctk_widget_get_visible (task->label))
    {
      if (task->resize_idle_id == 0)
        task->resize_idle_id = g_idle_add (task_button_queue_resize, task);
    }
}

static void
vnck_tasklist_size_allocate (CtkWidget      *widget,
                             CtkAllocation  *allocation)
{
  CtkAllocation child_allocation;
  VnckTasklist *tasklist;
  VnckTask *class_group_task;
  int n_windows;
  int n_startup_sequences;
  GList *l;
  int button_width;
  int total_width;
  int n_rows;
  int n_cols;
  int n_grouped_buttons;
  int i;
  gboolean score_set;
  GList *ungrouped_class_groups;
  VnckTask *win_task;
  GList *visible_tasks = NULL;
  GList *windows_sorted = NULL;
  int grouping_limit;

  if (allocation->width <= 1 || allocation->height <= 1)
    {
      CTK_WIDGET_CLASS (vnck_tasklist_parent_class)->size_allocate (widget, allocation);
      return;
    }

  tasklist = VNCK_TASKLIST (widget);

  n_windows = g_list_length (tasklist->priv->windows);
  n_startup_sequences = g_list_length (tasklist->priv->startup_sequences);
  n_grouped_buttons = 0;
  ungrouped_class_groups = g_list_copy (tasklist->priv->class_groups);
  score_set = FALSE;

  grouping_limit = MIN (tasklist->priv->grouping_limit,
			tasklist->priv->max_button_width);

  /* Try ungrouped mode */
  button_width = vnck_tasklist_layout (allocation,
				       tasklist->priv->max_button_width,
				       tasklist->priv->max_button_height,
				       n_startup_sequences + n_windows,
				       tasklist->priv->orientation,
				       &n_cols, &n_rows);
  while (ungrouped_class_groups != NULL &&
	 ((tasklist->priv->grouping == VNCK_TASKLIST_ALWAYS_GROUP) ||
	  ((tasklist->priv->grouping == VNCK_TASKLIST_AUTO_GROUP) &&
	   (button_width < grouping_limit))))
    {
      if (!score_set)
	{
	  vnck_tasklist_score_groups (tasklist, ungrouped_class_groups);
	  score_set = TRUE;
	}

      ungrouped_class_groups = vnck_task_get_highest_scored (ungrouped_class_groups, &class_group_task);

      n_grouped_buttons += g_list_length (class_group_task->windows) - 1;

      if (g_list_length (class_group_task->windows) > 1)
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task);

          /* Sort */
          class_group_task->windows = g_list_sort (class_group_task->windows,
                                                   vnck_task_compare_alphabetically);

	  /* Hide all this group's windows */
	  l = class_group_task->windows;
	  while (l != NULL)
	    {
	      win_task = VNCK_TASK (l->data);

	      ctk_widget_set_child_visible (CTK_WIDGET (win_task->button), FALSE);

	      l = l->next;
	    }
	}
      else
	{
	  visible_tasks = g_list_prepend (visible_tasks, class_group_task->windows->data);
	  ctk_widget_set_child_visible (CTK_WIDGET (class_group_task->button), FALSE);
        }

      button_width = vnck_tasklist_layout (allocation,
					   tasklist->priv->max_button_width,
					   tasklist->priv->max_button_height,
					   n_startup_sequences + n_windows - n_grouped_buttons,
					   tasklist->priv->orientation,
					   &n_cols, &n_rows);
    }

  /* Add all ungrouped windows to visible_tasks, and hide their class groups */
  l = ungrouped_class_groups;
  while (l != NULL)
    {
      class_group_task = VNCK_TASK (l->data);

      visible_tasks = g_list_concat (visible_tasks, g_list_copy (class_group_task->windows));
      ctk_widget_set_child_visible (CTK_WIDGET (class_group_task->button), FALSE);

      l = l->next;
    }

  /* Add all windows that are ungrouped because they don't belong to any class
   * group */
  l = tasklist->priv->windows_without_class_group;
  while (l != NULL)
    {
      VnckTask *task;

      task = VNCK_TASK (l->data);
      visible_tasks = g_list_append (visible_tasks, task);

      l = l->next;
    }

  /* Add all startup sequences */
  visible_tasks = g_list_concat (visible_tasks, g_list_copy (tasklist->priv->startup_sequences));

  /* Sort */
  visible_tasks = g_list_sort (visible_tasks, vnck_task_compare);

  /* Allocate children */
  l = visible_tasks;
  i = 0;
  total_width = tasklist->priv->max_button_width * n_cols;
  total_width = MIN (total_width, allocation->width);
  /* FIXME: this is obviously wrong, but if we don't this, some space that the
   * panel allocated to us won't have the panel popup menu, but the tasklist
   * popup menu */
  total_width = allocation->width;
  while (l != NULL)
    {
      VnckTask *task = VNCK_TASK (l->data);
      int row = i % n_rows;
      int col = i / n_rows;

      if (ctk_widget_get_direction (widget) == CTK_TEXT_DIR_RTL)
        col = n_cols - col - 1;

      child_allocation.x = total_width*col / n_cols;
      child_allocation.y = allocation->height*row / n_rows;
      child_allocation.width = total_width*(col + 1) / n_cols - child_allocation.x;
      child_allocation.height = allocation->height*(row + 1) / n_rows - child_allocation.y;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      ctk_widget_size_allocate (task->button, &child_allocation);
      ctk_widget_set_child_visible (CTK_WIDGET (task->button), TRUE);

      if (task->type != VNCK_TASK_STARTUP_SEQUENCE)
        {
          GList *ll;

          /* Build sorted windows list */
          if (g_list_length (task->windows) > 1)
            windows_sorted = g_list_concat (windows_sorted,
                                           g_list_copy (task->windows));
          else
            windows_sorted = g_list_append (windows_sorted, task);
          task->row = row;
          task->col = col;
          for (ll = task->windows; ll; ll = ll->next)
            {
              VNCK_TASK (ll->data)->row = row;
              VNCK_TASK (ll->data)->col = col;
            }
        }
      i++;
      l = l->next;
    }

  /* Update icon geometries. */
  vnck_tasklist_update_icon_geometries (tasklist, visible_tasks);

  g_list_free (visible_tasks);
  g_list_free (tasklist->priv->windows);
  g_list_free (ungrouped_class_groups);
  tasklist->priv->windows = windows_sorted;

  CTK_WIDGET_CLASS (vnck_tasklist_parent_class)->size_allocate (widget,
                                                                allocation);
}

static void
foreach_tasklist (VnckTasklist *tasklist,
		  gpointer      user_data G_GNUC_UNUSED)
{
  vnck_tasklist_update_lists (tasklist);
}

static void
vnck_tasklist_realize (CtkWidget *widget)
{
  VnckTasklist *tasklist;
  CdkScreen *cdkscreen;

  tasklist = VNCK_TASKLIST (widget);

  cdkscreen = ctk_widget_get_screen (widget);
  tasklist->priv->screen = vnck_screen_get (cdk_x11_screen_get_screen_number (cdkscreen));
  g_assert (tasklist->priv->screen != NULL);

#ifdef HAVE_STARTUP_NOTIFICATION
  tasklist->priv->sn_context =
    sn_monitor_context_new (_vnck_screen_get_sn_display (tasklist->priv->screen),
                            vnck_screen_get_number (tasklist->priv->screen),
                            vnck_tasklist_sn_event,
                            tasklist,
                            NULL);
#endif

  (* CTK_WIDGET_CLASS (vnck_tasklist_parent_class)->realize) (widget);

  tasklist_instances = g_slist_append (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances, (GFunc) foreach_tasklist, NULL);

  vnck_tasklist_update_lists (tasklist);

  vnck_tasklist_connect_screen (tasklist);
}

static void
vnck_tasklist_unrealize (CtkWidget *widget)
{
  VnckTasklist *tasklist;

  tasklist = VNCK_TASKLIST (widget);

  vnck_tasklist_disconnect_screen (tasklist);
  tasklist->priv->screen = NULL;

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_monitor_context_unref (tasklist->priv->sn_context);
  tasklist->priv->sn_context = NULL;
#endif

  (* CTK_WIDGET_CLASS (vnck_tasklist_parent_class)->unrealize) (widget);

  tasklist_instances = g_slist_remove (tasklist_instances, tasklist);
  g_slist_foreach (tasklist_instances, (GFunc) foreach_tasklist, NULL);
}

static void
vnck_tasklist_forall (CtkContainer *container,
		      gboolean      include_internals G_GNUC_UNUSED,
		      CtkCallback   callback,
		      gpointer      callback_data)
{
  VnckTasklist *tasklist;
  GList *tmp;

  tasklist = VNCK_TASKLIST (container);

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }

  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      (* callback) (task->button, callback_data);
    }
}

static void
vnck_tasklist_remove (CtkContainer   *container,
		      CtkWidget	     *widget)
{
  VnckTasklist *tasklist;
  GList *tmp;

  g_return_if_fail (VNCK_IS_TASKLIST (container));
  g_return_if_fail (widget != NULL);

  tasklist = VNCK_TASKLIST (container);

  /* it's safer to handle windows_without_class_group before windows */
  tmp = tasklist->priv->windows_without_class_group;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  tasklist->priv->windows_without_class_group =
	    g_list_remove (tasklist->priv->windows_without_class_group,
			   task);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->windows;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->win_hash,
			       task->window);
	  tasklist->priv->windows =
	    g_list_remove (tasklist->priv->windows,
			   task);

          ctk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->class_groups;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  g_hash_table_remove (tasklist->priv->class_group_hash,
			       task->class_group);
	  tasklist->priv->class_groups =
	    g_list_remove (tasklist->priv->class_groups,
			   task);

          ctk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      tmp = tmp->next;

      if (task->button == widget)
	{
	  tasklist->priv->startup_sequences =
	    g_list_remove (tasklist->priv->startup_sequences,
			   task);

          ctk_widget_unparent (widget);
          g_object_unref (task);
	  break;
	}
    }

  ctk_widget_queue_resize (CTK_WIDGET (container));
}

static void
vnck_tasklist_connect_screen (VnckTasklist *tasklist)
{
  GList *windows;
  guint *c;
  int    i;
  VnckScreen *screen;

  g_return_if_fail (tasklist->priv->screen != NULL);

  screen = tasklist->priv->screen;

  i = 0;
  c = tasklist->priv->screen_connections;

  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_window_changed",
                                     G_CALLBACK (vnck_tasklist_active_window_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "active_workspace_changed",
                                     G_CALLBACK (vnck_tasklist_active_workspace_changed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_opened",
                                     G_CALLBACK (vnck_tasklist_window_added),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "window_closed",
                                     G_CALLBACK (vnck_tasklist_window_removed),
                                     tasklist, 0);
  c [i++] = g_signal_connect_object (G_OBJECT (screen), "viewports_changed",
                                     G_CALLBACK (vnck_tasklist_viewports_changed),
                                     tasklist, 0);


  g_assert (i == N_SCREEN_CONNECTIONS);

  windows = vnck_screen_get_windows (screen);
  while (windows != NULL)
    {
      vnck_tasklist_connect_window (tasklist, windows->data);
      windows = windows->next;
    }
}

static void
vnck_tasklist_disconnect_screen (VnckTasklist *tasklist)
{
  GList *windows;
  int    i;

  windows = vnck_screen_get_windows (tasklist->priv->screen);
  while (windows != NULL)
    {
      vnck_tasklist_disconnect_window (tasklist, windows->data);
      windows = windows->next;
    }

  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (tasklist->priv->screen_connections [i] != 0)
        g_signal_handler_disconnect (G_OBJECT (tasklist->priv->screen),
                                     tasklist->priv->screen_connections [i]);

      tasklist->priv->screen_connections [i] = 0;

      ++i;
    }

  g_assert (i == N_SCREEN_CONNECTIONS);

#ifdef HAVE_STARTUP_NOTIFICATION
  if (tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
#endif
}

static gboolean
vnck_tasklist_scroll_event (CtkWidget      *widget,
                            CdkEventScroll *event)
{
  /* use the fact that tasklist->priv->windows is sorted
   * see vnck_tasklist_size_allocate() */
  VnckTasklist *tasklist;
  CtkTextDirection ltr;
  GList *window;
  gint row = 0;
  gint col = 0;

  tasklist = VNCK_TASKLIST (widget);

  if (!tasklist->priv->scroll_enabled)
    return FALSE;

  window = g_list_find (tasklist->priv->windows,
                        tasklist->priv->active_task);
  if (window)
    {
      row = VNCK_TASK (window->data)->row;
      col = VNCK_TASK (window->data)->col;
    }
  else
    if (tasklist->priv->activate_timeout_id)
      /* There is no active_task yet, but there will be one after the timeout.
       * It occurs if we change the active task too fast. */
      return TRUE;

  ltr = (ctk_widget_get_direction (CTK_WIDGET (tasklist)) != CTK_TEXT_DIR_RTL);

  switch (event->direction)
    {
      case CDK_SCROLL_UP:
        if (!window)
          window = g_list_last (tasklist->priv->windows);
        else
          window = window->prev;
      break;

      case CDK_SCROLL_DOWN:
        if (!window)
          window = tasklist->priv->windows;
        else
          window = window->next;
      break;

#define TASKLIST_GET_MOST_LEFT(ltr, window, tasklist)   \
  do                                                    \
    {                                                   \
      if (ltr)                                          \
        window = tasklist->priv->windows;               \
      else                                              \
        window = g_list_last (tasklist->priv->windows); \
    } while (0)

#define TASKLIST_GET_MOST_RIGHT(ltr, window, tasklist)  \
  do                                                    \
    {                                                   \
      if (ltr)                                          \
        window = g_list_last (tasklist->priv->windows); \
      else                                              \
        window = tasklist->priv->windows;               \
    } while (0)

      case CDK_SCROLL_LEFT:
        if (!window)
          TASKLIST_GET_MOST_RIGHT (ltr, window, tasklist);
        else
          {
            /* Search the first window on the previous colomn at same row */
            if (ltr)
              {
                while (window && (VNCK_TASK(window->data)->row != row ||
                                  VNCK_TASK(window->data)->col != col-1))
                  window = window->prev;
              }
            else
              {
                while (window && (VNCK_TASK(window->data)->row != row ||
                                  VNCK_TASK(window->data)->col != col-1))
                  window = window->next;
              }
            /* If no window found, select the top/bottom left one */
            if (!window)
              TASKLIST_GET_MOST_LEFT (ltr, window, tasklist);
          }
      break;

      case CDK_SCROLL_RIGHT:
        if (!window)
          TASKLIST_GET_MOST_LEFT (ltr, window, tasklist);
        else
          {
            /* Search the first window on the next colomn at same row */
            if (ltr)
              {
                while (window && (VNCK_TASK(window->data)->row != row ||
                                  VNCK_TASK(window->data)->col != col+1))
                  window = window->next;
              }
            else
              {
                while (window && (VNCK_TASK(window->data)->row != row ||
                                  VNCK_TASK(window->data)->col != col+1))
                  window = window->prev;
              }
            /* If no window found, select the top/bottom right one */
            if (!window)
              TASKLIST_GET_MOST_RIGHT (ltr, window, tasklist);
          }
      break;

      case CDK_SCROLL_SMOOTH:
        window = NULL;
      break;

#undef TASKLIST_GET_MOST_LEFT
#undef TASKLIST_GET_MOST_RIGHT

      default:
        g_assert_not_reached ();
    }

  if (window)
    vnck_tasklist_activate_task_window (window->data, event->time);

  return TRUE;
}

/**
 * vnck_tasklist_new:
 *
 * Creates a new #VnckTasklist. The #VnckTasklist will list #VnckWindow of the
 * #VnckScreen it is on.
 *
 * Return value: a newly created #VnckTasklist.
 */
CtkWidget*
vnck_tasklist_new (void)
{
  VnckTasklist *tasklist;

  tasklist = g_object_new (VNCK_TYPE_TASKLIST, NULL);

  return CTK_WIDGET (tasklist);
}

static void
vnck_tasklist_free_tasks (VnckTasklist *tasklist)
{
  GList *l;

  tasklist->priv->active_task = NULL;
  tasklist->priv->active_class_group = NULL;

  if (tasklist->priv->windows)
    {
      l = tasklist->priv->windows;
      while (l != NULL)
	{
	  VnckTask *task = VNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  ctk_widget_destroy (task->button);
	}
    }
  g_assert (tasklist->priv->windows == NULL);
  g_assert (tasklist->priv->windows_without_class_group == NULL);
  g_assert (g_hash_table_size (tasklist->priv->win_hash) == 0);

  if (tasklist->priv->class_groups)
    {
      l = tasklist->priv->class_groups;
      while (l != NULL)
	{
	  VnckTask *task = VNCK_TASK (l->data);
	  l = l->next;
          /* if we just unref the task it means we lose our ref to the
           * task before we unparent the button, which breaks stuff.
           */
	  ctk_widget_destroy (task->button);
	}
    }

  g_assert (tasklist->priv->class_groups == NULL);
  g_assert (g_hash_table_size (tasklist->priv->class_group_hash) == 0);

  if (tasklist->priv->skipped_windows)
    {
      vnck_tasklist_free_skipped_windows (tasklist);
      tasklist->priv->skipped_windows = NULL;
    }
}


/*
 * This function determines if a window should be included in the tasklist.
 */
static gboolean
tasklist_include_window_impl (VnckTasklist *tasklist,
                              VnckWindow   *win,
                              gboolean      check_for_skipped_list)
{
  VnckWorkspace *active_workspace;
  int x, y, w, h;

  if (!check_for_skipped_list &&
      vnck_window_get_state (win) & VNCK_WINDOW_STATE_SKIP_TASKLIST)
    return FALSE;

  if (tasklist->priv->monitor != NULL)
    {
      CdkDisplay *display;
      CdkMonitor *monitor;

      vnck_window_get_geometry (win, &x, &y, &w, &h);

      /* Don't include the window if its center point is not on the same monitor */

      display = cdk_display_get_default ();
      monitor = cdk_display_get_monitor_at_point (display, x + w / 2, y + h / 2);

      if (monitor != tasklist->priv->monitor)
        return FALSE;
    }

  /* Remainder of checks aren't relevant for checking if the window should
   * be in the skipped list.
   */
  if (check_for_skipped_list)
    return TRUE;

  if (tasklist->priv->include_all_workspaces)
    return TRUE;

  if (vnck_window_is_pinned (win))
    return TRUE;

  active_workspace = vnck_screen_get_active_workspace (tasklist->priv->screen);
  if (active_workspace == NULL)
    return TRUE;

  if (vnck_window_or_transient_needs_attention (win))
    return TRUE;

  if (active_workspace != vnck_window_get_workspace (win))
    return FALSE;

  if (!vnck_workspace_is_virtual (active_workspace))
    return TRUE;

  return vnck_window_is_in_viewport (win, active_workspace);
}

static gboolean
tasklist_include_in_skipped_list (VnckTasklist *tasklist, VnckWindow *win)
{
  return tasklist_include_window_impl (tasklist,
                                       win,
                                       TRUE /* check_for_skipped_list */);
}

static gboolean
vnck_tasklist_include_window (VnckTasklist *tasklist, VnckWindow *win)
{
  return tasklist_include_window_impl (tasklist,
                                       win,
                                       FALSE /* check_for_skipped_list */);
}

static void
vnck_tasklist_update_lists (VnckTasklist *tasklist)
{
  CdkWindow *tasklist_window;
  GList *windows;
  VnckWindow *win;
  VnckClassGroup *class_group;
  GList *l;
  VnckTask *win_task;
  VnckTask *class_group_task;

  vnck_tasklist_free_tasks (tasklist);

  /* vnck_tasklist_update_lists() will be called on realize */
  if (!ctk_widget_get_realized (CTK_WIDGET (tasklist)))
    return;

  tasklist_window = ctk_widget_get_window (CTK_WIDGET (tasklist));

  if (tasklist_window != NULL)
    {
      /*
       * only show windows from this monitor if there is more than one tasklist running
       */
      if (tasklist_instances == NULL || tasklist_instances->next == NULL)
        {
          tasklist->priv->monitor = NULL;
        }
      else
        {
          CdkDisplay *display;
          CdkMonitor *monitor;

          display = cdk_display_get_default ();
          monitor = cdk_display_get_monitor_at_window (display, tasklist_window);

          if (monitor != tasklist->priv->monitor)
            {
              tasklist->priv->monitor = monitor;
              cdk_monitor_get_geometry (monitor, &tasklist->priv->monitor_geometry);
            }
        }
    }

  l = windows = vnck_screen_get_windows (tasklist->priv->screen);
  while (l != NULL)
    {
      win = VNCK_WINDOW (l->data);

      if (vnck_tasklist_include_window (tasklist, win))
	{
	  win_task = vnck_task_new_from_window (tasklist, win);
	  tasklist->priv->windows = g_list_prepend (tasklist->priv->windows, win_task);
	  g_hash_table_insert (tasklist->priv->win_hash, win, win_task);

	  ctk_widget_set_parent (win_task->button, CTK_WIDGET (tasklist));
	  ctk_widget_show (win_task->button);

	  /* Class group */

	  class_group = vnck_window_get_class_group (win);
          /* don't group windows if they do not belong to any class */
          if (strcmp (vnck_class_group_get_id (class_group), "") != 0)
            {
              class_group_task =
                        g_hash_table_lookup (tasklist->priv->class_group_hash,
                                             class_group);

              if (class_group_task == NULL)
                {
                  class_group_task =
                                  vnck_task_new_from_class_group (tasklist,
                                                                  class_group);
                  ctk_widget_set_parent (class_group_task->button,
                                         CTK_WIDGET (tasklist));
                  ctk_widget_show (class_group_task->button);

                  tasklist->priv->class_groups =
                                  g_list_prepend (tasklist->priv->class_groups,
                                                  class_group_task);
                  g_hash_table_insert (tasklist->priv->class_group_hash,
                                       class_group, class_group_task);
                }

              class_group_task->windows =
                                    g_list_prepend (class_group_task->windows,
                                                    win_task);
            }
          else
            {
              g_object_ref (win_task);
              tasklist->priv->windows_without_class_group =
                              g_list_prepend (tasklist->priv->windows_without_class_group,
                                              win_task);
            }
	}
      else if (tasklist_include_in_skipped_list (tasklist, win))
        {
          skipped_window *skipped = g_new0 (skipped_window, 1);
          skipped->window = g_object_ref (win);
          skipped->tag = g_signal_connect (G_OBJECT (win),
                                           "state_changed",
                                           G_CALLBACK (vnck_task_state_changed),
                                           tasklist);
          tasklist->priv->skipped_windows =
            g_list_prepend (tasklist->priv->skipped_windows,
                            (gpointer) skipped);
        }

      l = l->next;
    }

  /* Sort the class group list */
  l = tasklist->priv->class_groups;
  while (l)
    {
      class_group_task = VNCK_TASK (l->data);

      class_group_task->windows = g_list_sort (class_group_task->windows, vnck_task_compare);

      /* so the number of windows in the task gets reset on the
       * task label
       */
      vnck_task_update_visible_state (class_group_task);

      l = l->next;
    }

  /* since we cleared active_window we need to reset it */
  vnck_tasklist_active_window_changed (tasklist->priv->screen, NULL, tasklist);

  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static void
vnck_tasklist_change_active_task (VnckTasklist *tasklist, VnckTask *active_task)
{
  if (active_task &&
      active_task == tasklist->priv->active_task)
    return;

  g_assert (active_task == NULL ||
            active_task->type != VNCK_TASK_STARTUP_SEQUENCE);

  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    FALSE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }

  tasklist->priv->active_task = active_task;

  if (tasklist->priv->active_task)
    {
      tasklist->priv->active_task->really_toggling = TRUE;
      ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (tasklist->priv->active_task->button),
				    TRUE);
      tasklist->priv->active_task->really_toggling = FALSE;
    }

  if (active_task)
    {
      active_task = g_hash_table_lookup (tasklist->priv->class_group_hash,
					 active_task->class_group);

      if (active_task &&
	  active_task == tasklist->priv->active_class_group)
	return;

      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					FALSE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}

      tasklist->priv->active_class_group = active_task;

      if (tasklist->priv->active_class_group)
	{
	  tasklist->priv->active_class_group->really_toggling = TRUE;
	  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (tasklist->priv->active_class_group->button),
					TRUE);
	  tasklist->priv->active_class_group->really_toggling = FALSE;
	}
    }
}

static void
vnck_tasklist_update_icon_geometries (VnckTasklist *tasklist G_GNUC_UNUSED,
				      GList        *visible_tasks)
{
	gint x, y, width, height;
	GList *l1;

	for (l1 = visible_tasks; l1; l1 = l1->next) {
		VnckTask *task = VNCK_TASK (l1->data);
                CtkAllocation allocation;

		if (!ctk_widget_get_realized (task->button))
			continue;

                /* Let's cheat with some internal knowledge of CtkButton: in a
                 * CtkButton, the window is the same as the parent window. So
                 * to know the position of the widget, we should use the
                 * the position of the parent window and the allocation information. */

                ctk_widget_get_allocation (task->button, &allocation);

                cdk_window_get_origin (ctk_widget_get_parent_window (task->button),
                                       &x, &y);

                x += allocation.x;
                y += allocation.y;
                width = allocation.width;
                height = allocation.height;

		if (task->window)
			vnck_window_set_icon_geometry (task->window,
						       x, y, width, height);
		else {
			GList *l2;

			for (l2 = task->windows; l2; l2 = l2->next) {
				VnckTask *win_task = VNCK_TASK (l2->data);

				g_assert (win_task->window);

				vnck_window_set_icon_geometry (win_task->window,
							       x, y, width, height);
			}
		}
	}
}

static void
vnck_tasklist_active_window_changed (VnckScreen   *screen,
				     VnckWindow   *previous_window G_GNUC_UNUSED,
				     VnckTasklist *tasklist)
{
  VnckWindow *active_window;
  VnckWindow *initial_window;
  VnckTask *active_task = NULL;

  /* FIXME: check for group modal window */
  initial_window = active_window = vnck_screen_get_active_window (screen);
  active_task = g_hash_table_lookup (tasklist->priv->win_hash, active_window);
  while (active_window && !active_task)
    {
      active_window = vnck_window_get_transient (active_window);
      active_task = g_hash_table_lookup (tasklist->priv->win_hash,
                                         active_window);
      /* Check for transient cycles */
      if (active_window == initial_window)
        break;
    }

  vnck_tasklist_change_active_task (tasklist, active_task);
}

static void
vnck_tasklist_active_workspace_changed (VnckScreen    *screen G_GNUC_UNUSED,
					VnckWorkspace *previous_workspace G_GNUC_UNUSED,
					VnckTasklist  *tasklist)
{
  vnck_tasklist_update_lists (tasklist);
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static void
vnck_tasklist_window_changed_workspace (VnckWindow   *window,
					VnckTasklist *tasklist)
{
  VnckWorkspace *active_ws;
  VnckWorkspace *window_ws;
  gboolean       need_update;
  GList         *l;

  active_ws = vnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = vnck_window_get_workspace (window);

  if (!window_ws)
    return;

  need_update = FALSE;

  if (active_ws == window_ws)
    need_update = TRUE;

  l = tasklist->priv->windows;
  while (!need_update && l != NULL)
    {
      VnckTask *task = l->data;

      if (task->type == VNCK_TASK_WINDOW &&
          task->window == window)
        need_update = TRUE;

      l = l->next;
    }

  if (need_update)
    {
      vnck_tasklist_update_lists (tasklist);
      ctk_widget_queue_resize (CTK_WIDGET (tasklist));
    }
}

static gboolean
do_vnck_tasklist_update_lists (gpointer data)
{
  VnckTasklist *tasklist = VNCK_TASKLIST (data);

  tasklist->priv->idle_callback_tag = 0;

  vnck_tasklist_update_lists (tasklist);

  return FALSE;
}

static void
vnck_tasklist_window_changed_geometry (VnckWindow   *window,
				       VnckTasklist *tasklist)
{
  CdkWindow *tasklist_window;
  VnckTask *win_task;
  gboolean show;
  gboolean monitor_changed;
  int x, y, w, h;

  if (tasklist->priv->idle_callback_tag != 0)
    return;

  tasklist_window = ctk_widget_get_window (CTK_WIDGET (tasklist));

  /*
   * If the (parent of the) tasklist itself skips
   * the tasklist, we need an extra check whether
   * the tasklist itself possibly changed monitor.
   */
  monitor_changed = FALSE;
  if (tasklist->priv->monitor != NULL &&
      (vnck_window_get_state (window) & VNCK_WINDOW_STATE_SKIP_TASKLIST) &&
      tasklist_window != NULL)
    {
      /* Do the extra check only if there is a suspect of a monitor change (= this window is off monitor) */
      vnck_window_get_geometry (window, &x, &y, &w, &h);
      if (!POINT_IN_RECT (x + w / 2, y + h / 2, tasklist->priv->monitor_geometry))
        {
          CdkDisplay *display;
          CdkMonitor *monitor;

          display = cdk_display_get_default ();
          monitor = cdk_display_get_monitor_at_window (display, tasklist_window);

          monitor_changed = (monitor != tasklist->priv->monitor);
        }
    }

  /*
   * We want to re-generate the task list if
   * the window is shown but shouldn't be or
   * the window isn't shown but should be or
   * the tasklist itself changed monitor.
   */
  win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);
  show = vnck_tasklist_include_window (tasklist, window);
  if (((win_task == NULL && !show) || (win_task != NULL && show)) &&
      !monitor_changed)
    return;

  /* Don't keep any stale references */
  ctk_widget_queue_draw (CTK_WIDGET (tasklist));

  tasklist->priv->idle_callback_tag = g_idle_add (do_vnck_tasklist_update_lists, tasklist);
}

static void
vnck_tasklist_connect_window (VnckTasklist *tasklist,
			      VnckWindow   *window)
{
  g_signal_connect_object (window, "workspace_changed",
			   G_CALLBACK (vnck_tasklist_window_changed_workspace),
			   tasklist, 0);
  g_signal_connect_object (window, "geometry_changed",
			   G_CALLBACK (vnck_tasklist_window_changed_geometry),
			   tasklist, 0);
}

static void
vnck_tasklist_disconnect_window (VnckTasklist *tasklist,
			         VnckWindow   *window)
{
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_tasklist_window_changed_workspace,
                                        tasklist);
  g_signal_handlers_disconnect_by_func (window,
                                        vnck_tasklist_window_changed_geometry,
                                        tasklist);
}

static void
vnck_tasklist_window_added (VnckScreen   *screen G_GNUC_UNUSED,
			    VnckWindow   *win,
			    VnckTasklist *tasklist)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  vnck_tasklist_check_end_sequence (tasklist, win);
#endif

  vnck_tasklist_connect_window (tasklist, win);

  vnck_tasklist_update_lists (tasklist);
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static void
vnck_tasklist_window_removed (VnckScreen   *screen G_GNUC_UNUSED,
			      VnckWindow   *win G_GNUC_UNUSED,
			      VnckTasklist *tasklist)
{
  vnck_tasklist_update_lists (tasklist);
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static void
vnck_tasklist_viewports_changed (VnckScreen   *screen G_GNUC_UNUSED,
				 VnckTasklist *tasklist)
{
  vnck_tasklist_update_lists (tasklist);
  ctk_widget_queue_resize (CTK_WIDGET (tasklist));
}

static gboolean
vnck_tasklist_change_active_timeout (gpointer data)
{
  VnckTasklist *tasklist = VNCK_TASKLIST (data);

  tasklist->priv->activate_timeout_id = 0;

  vnck_tasklist_active_window_changed (tasklist->priv->screen, NULL, tasklist);

  return FALSE;
}

static void
vnck_task_menu_activated (CtkMenuItem *menu_item G_GNUC_UNUSED,
			  gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);

  /* This is an "activate" callback function so ctk_get_current_event_time()
   * will suffice.
   */
  vnck_tasklist_activate_task_window (task, ctk_get_current_event_time ());
}

static void
vnck_tasklist_activate_next_in_class_group (VnckTask *task,
                                            guint32   timestamp)
{
  VnckTask *activate_task;
  gboolean  activate_next;
  GList    *l;

  activate_task = NULL;
  activate_next = FALSE;

  l = task->windows;
  while (l)
    {
      VnckTask *tmp;

      tmp = VNCK_TASK (l->data);

      if (vnck_window_is_most_recently_activated (tmp->window))
        activate_next = TRUE;
      else if (activate_next)
        {
          activate_task = tmp;
          break;
        }

      l = l->next;
    }

  /* no task in this group is active, or only the last one => activate
   * the first task */
  if (!activate_task && task->windows)
    activate_task = VNCK_TASK (task->windows->data);

  if (activate_task)
    {
      task->was_active = FALSE;
      vnck_tasklist_activate_task_window (activate_task, timestamp);
    }
}

static void
vnck_tasklist_activate_task_window (VnckTask *task,
                                    guint32   timestamp)
{
  VnckTasklist *tasklist;
  VnckWindowState state;
  VnckWorkspace *active_ws;
  VnckWorkspace *window_ws;

  tasklist = task->tasklist;

  if (task->window == NULL)
    return;

  state = vnck_window_get_state (task->window);

  active_ws = vnck_screen_get_active_workspace (tasklist->priv->screen);
  window_ws = vnck_window_get_workspace (task->window);

  if (state & VNCK_WINDOW_STATE_MINIMIZED)
    {
      if (window_ws &&
          active_ws != window_ws &&
          !tasklist->priv->switch_workspace_on_unminimize)
        vnck_workspace_activate (window_ws, timestamp);

      vnck_window_activate_transient (task->window, timestamp);
    }
  else
    {
      if ((task->was_active ||
           vnck_window_transient_is_most_recently_activated (task->window)) &&
          (!window_ws || active_ws == window_ws))
	{
	  task->was_active = FALSE;
	  vnck_window_minimize (task->window);
	  return;
	}
      else
	{
          /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
           * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
           * There should only be *one* activate call.
           */
          if (window_ws)
            vnck_workspace_activate (window_ws, timestamp);

          vnck_window_activate_transient (task->window, timestamp);
        }
    }


  if (tasklist->priv->activate_timeout_id)
    g_source_remove (tasklist->priv->activate_timeout_id);

  tasklist->priv->activate_timeout_id =
    g_timeout_add (500, &vnck_tasklist_change_active_timeout, tasklist);

  vnck_tasklist_change_active_task (tasklist, task);
}

static void
vnck_task_close_all (CtkMenuItem *menu_item G_GNUC_UNUSED,
		     gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      VnckTask *child = VNCK_TASK (l->data);
      vnck_window_close (child->window, ctk_get_current_event_time ());
      l = l->next;
    }
}

static void
vnck_task_unminimize_all (CtkMenuItem *menu_item G_GNUC_UNUSED,
			  gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      VnckTask *child = VNCK_TASK (l->data);
      /* This is inside an activate callback, so ctk_get_current_event_time()
       * will work.
       */
      vnck_window_unminimize (child->window, ctk_get_current_event_time ());
      l = l->next;
    }
}

static void
vnck_task_minimize_all (CtkMenuItem *menu_item G_GNUC_UNUSED,
			gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      VnckTask *child = VNCK_TASK (l->data);
      vnck_window_minimize (child->window);
      l = l->next;
    }
}

static void
vnck_task_unmaximize_all (CtkMenuItem *menu_item G_GNUC_UNUSED,
			  gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      VnckTask *child = VNCK_TASK (l->data);
      vnck_window_unmaximize (child->window);
      l = l->next;
    }
}

static void
vnck_task_maximize_all (CtkMenuItem *menu_item G_GNUC_UNUSED,
			gpointer     data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *l;

  l = task->windows;
  while (l)
    {
      VnckTask *child = VNCK_TASK (l->data);
      vnck_window_maximize (child->window);
      l = l->next;
    }
}

static void
vnck_task_popup_menu (VnckTask *task,
                      gboolean  action_submenu)
{
  CtkWidget *menu;
  VnckTask *win_task;
  char *text;
  GdkPixbuf *pixbuf;
  CtkWidget *menu_item;
  GList *l, *list;

  g_return_if_fail (task->type == VNCK_TASK_CLASS_GROUP);

  if (task->class_group == NULL)
    return;

  if (task->menu == NULL)
    {
      task->menu = ctk_menu_new ();
      g_object_ref_sink (task->menu);
    }

  menu = task->menu;

  /* Remove old menu content */
  list = ctk_container_get_children (CTK_CONTAINER (menu));
  l = list;
  while (l)
    {
      CtkWidget *child = CTK_WIDGET (l->data);
      ctk_container_remove (CTK_CONTAINER (menu), child);
      l = l->next;
    }
  g_list_free (list);

  l = task->windows;
  while (l)
    {
      win_task = VNCK_TASK (l->data);

      text = vnck_task_get_text (win_task, TRUE, TRUE);
      menu_item = vnck_image_menu_item_new_with_label (text);
      g_free (text);

      if (vnck_task_get_needs_attention (win_task))
        _make_ctk_label_bold (CTK_LABEL (ctk_bin_get_child (CTK_BIN (menu_item))));

      text = vnck_task_get_text (win_task, FALSE, FALSE);
      ctk_widget_set_tooltip_text (menu_item, text);
      g_free (text);

      pixbuf = vnck_task_get_icon (win_task);
      if (pixbuf)
        {
          VnckImageMenuItem *item;

          item = VNCK_IMAGE_MENU_ITEM (menu_item);

          vnck_image_menu_item_set_image_from_icon_pixbuf (item, pixbuf);
          g_object_unref (pixbuf);
        }

      ctk_widget_show (menu_item);

      if (action_submenu)
        ctk_menu_item_set_submenu (CTK_MENU_ITEM (menu_item),
                                   vnck_action_menu_new (win_task->window));
      else
        {
          static const CtkTargetEntry targets[] = {
            { (gchar *) "application/x-vnck-window-id", 0, 0 }
          };

          g_signal_connect_object (G_OBJECT (menu_item), "activate",
                                   G_CALLBACK (vnck_task_menu_activated),
                                   G_OBJECT (win_task),
                                   0);


          ctk_drag_source_set (menu_item, CDK_BUTTON1_MASK,
                               targets, 1, CDK_ACTION_MOVE);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_begin",
                                   G_CALLBACK (vnck_task_drag_begin),
                                   G_OBJECT (win_task),
                                   0);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_end",
                                   G_CALLBACK (vnck_task_drag_end),
                                   G_OBJECT (win_task),
                                   0);
          g_signal_connect_object (G_OBJECT(menu_item), "drag_data_get",
                                   G_CALLBACK (vnck_task_drag_data_get),
                                   G_OBJECT (win_task),
                                   0);
        }

      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);

      l = l->next;
    }

  /* In case of Right click, show Minimize All, Unminimize All, Close All*/
  if (action_submenu)
    {
      CtkWidget *separator;

      separator = ctk_separator_menu_item_new ();
      ctk_widget_show (separator);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), separator);

      menu_item = ctk_menu_item_new_with_mnemonic (_("Mi_nimize All"));
      ctk_widget_show (menu_item);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
	    		       G_CALLBACK (vnck_task_minimize_all),
			       G_OBJECT (task),
			       0);

      menu_item =  ctk_menu_item_new_with_mnemonic (_("Un_minimize All"));
      ctk_widget_show (menu_item);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (vnck_task_unminimize_all),
			       G_OBJECT (task),
			       0);

      menu_item = ctk_menu_item_new_with_mnemonic (_("Ma_ximize All"));
      ctk_widget_show (menu_item);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (vnck_task_maximize_all),
			       G_OBJECT (task),
			       0);

      menu_item =  ctk_menu_item_new_with_mnemonic (_("_Unmaximize All"));
      ctk_widget_show (menu_item);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
  			       G_CALLBACK (vnck_task_unmaximize_all),
			       G_OBJECT (task),
			       0);

      separator = ctk_separator_menu_item_new ();
      ctk_widget_show (separator);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), separator);

      menu_item = ctk_menu_item_new_with_mnemonic(_("_Close All"));
      ctk_widget_show (menu_item);
      ctk_menu_shell_append (CTK_MENU_SHELL (menu), menu_item);
      g_signal_connect_object (G_OBJECT (menu_item), "activate",
			       G_CALLBACK (vnck_task_close_all),
			       G_OBJECT (task),
			       0);
    }

  ctk_menu_set_screen (CTK_MENU (menu),
		       _vnck_screen_get_cdk_screen (task->tasklist->priv->screen));

  ctk_widget_show (menu);
  ctk_menu_popup_at_widget (CTK_MENU (menu), task->button,
                            CDK_GRAVITY_SOUTH_WEST,
                            CDK_GRAVITY_NORTH_WEST,
                            NULL);
}

static void
vnck_task_button_toggled (CtkButton *button,
			  VnckTask  *task)
{
  /* Did we really want to change the state of the togglebutton? */
  if (task->really_toggling)
    return;

  /* Undo the toggle */
  task->really_toggling = TRUE;
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (button),
				!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (button)));
  task->really_toggling = FALSE;

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      vnck_task_popup_menu (task, FALSE);
      break;
    case VNCK_TASK_WINDOW:
      if (task->window == NULL)
	return;

      /* This should only be called by clicking on the task button, so
       * ctk_get_current_event_time() should be fine here...
       */
      vnck_tasklist_activate_task_window (task, ctk_get_current_event_time ());
      break;
    case VNCK_TASK_STARTUP_SEQUENCE:
      break;
    default:
      break;
    }
}

static char *
vnck_task_get_text (VnckTask *task,
                    gboolean  icon_text,
                    gboolean  include_state)
{
  const char *name;

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      name = vnck_class_group_get_name (task->class_group);
      if (name[0] != 0)
	return g_strdup_printf ("%s (%d)",
				name,
				g_list_length (task->windows));
      else
	return g_strdup_printf ("(%d)",
				g_list_length (task->windows));

    case VNCK_TASK_WINDOW:
      return _vnck_window_get_name_for_display (task->window,
                                                icon_text, include_state);
      break;

    case VNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      name = sn_startup_sequence_get_description (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_name (task->startup_sequence);
      if (name == NULL)
        name = sn_startup_sequence_get_binary_name (task->startup_sequence);

      return g_strdup (name);
#else
      return NULL;
#endif
      break;

    default:
      break;
    }

  return NULL;
}

static void
vnck_dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;

  g_assert (pixbuf != NULL);

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;

      for (x = 0; x < w; x++)
	{
	  pixels[3] /= 2;

	  pixels += pixel_stride;
	}

      row += row_stride;
    }
}

static GdkPixbuf *
vnck_task_scale_icon (GdkPixbuf *orig, gboolean minimized)
{
  int w, h;
  GdkPixbuf *pixbuf;

  if (!orig)
    return NULL;

  w = gdk_pixbuf_get_width (orig);
  h = gdk_pixbuf_get_height (orig);

  if (h != (int) MINI_ICON_SIZE ||
      !gdk_pixbuf_get_has_alpha (orig))
    {
      double scale;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       TRUE,
			       8,
			       MINI_ICON_SIZE * w / (double) h,
			       MINI_ICON_SIZE);

      scale = MINI_ICON_SIZE / (double) gdk_pixbuf_get_height (orig);

      gdk_pixbuf_scale (orig,
			pixbuf,
			0, 0,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			0, 0,
			scale, scale,
			GDK_INTERP_HYPER);
    }
  else
    pixbuf = orig;

  if (minimized)
    {
      if (orig == pixbuf)
	pixbuf = gdk_pixbuf_copy (orig);

      vnck_dimm_icon (pixbuf);
    }

  if (orig == pixbuf)
    g_object_ref (pixbuf);

  return pixbuf;
}


static GdkPixbuf *
vnck_task_get_icon (VnckTask *task)
{
  VnckWindowState state;
  GdkPixbuf *pixbuf;

  pixbuf = NULL;

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      pixbuf = vnck_task_scale_icon (vnck_class_group_get_mini_icon (task->class_group),
				     FALSE);
      break;

    case VNCK_TASK_WINDOW:
      state = vnck_window_get_state (task->window);

      pixbuf =  vnck_task_scale_icon (vnck_window_get_mini_icon (task->window),
				      state & VNCK_WINDOW_STATE_MINIMIZED);
      break;

    case VNCK_TASK_STARTUP_SEQUENCE:
#ifdef HAVE_STARTUP_NOTIFICATION
      if (task->tasklist->priv->icon_loader != NULL)
        {
          const char *icon;

          icon = sn_startup_sequence_get_icon_name (task->startup_sequence);
          if (icon != NULL)
            {
              GdkPixbuf *loaded;

              loaded =  (* task->tasklist->priv->icon_loader) (icon,
                                                               MINI_ICON_SIZE,
                                                               0,
                                                               task->tasklist->priv->icon_loader_data);

              if (loaded != NULL)
                {
                  pixbuf = vnck_task_scale_icon (loaded, FALSE);
                  g_object_unref (G_OBJECT (loaded));
                }
            }
        }

      if (pixbuf == NULL)
        {
          _vnck_get_fallback_icons (NULL, 0, 0,
                                    &pixbuf, MINI_ICON_SIZE, MINI_ICON_SIZE);
        }
#endif
      break;

    default:
      break;
    }

  return pixbuf;
}

static gboolean
vnck_task_get_needs_attention (VnckTask *task)
{
  GList *l;
  VnckTask *win_task;
  gboolean needs_attention;

  needs_attention = FALSE;

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      task->start_needs_attention = 0;
      l = task->windows;
      while (l)
	{
	  win_task = VNCK_TASK (l->data);

	  if (vnck_window_or_transient_needs_attention (win_task->window))
	    {
	      needs_attention = TRUE;
              task->start_needs_attention = MAX (task->start_needs_attention, _vnck_window_or_transient_get_needs_attention_time (win_task->window));
	      break;
	    }

	  l = l->next;
	}
      break;

    case VNCK_TASK_WINDOW:
      needs_attention =
	vnck_window_or_transient_needs_attention (task->window);
      task->start_needs_attention = _vnck_window_or_transient_get_needs_attention_time (task->window);
      break;

    case VNCK_TASK_STARTUP_SEQUENCE:
    default:
      break;
    }

  return needs_attention != FALSE;
}

static void
vnck_task_update_visible_state (VnckTask *task)
{
  GdkPixbuf *pixbuf;
  char *text;

  pixbuf = vnck_task_get_icon (task);
  ctk_image_set_from_pixbuf (CTK_IMAGE (task->image),
			     pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

  text = vnck_task_get_text (task, TRUE, TRUE);
  if (text != NULL)
    {
      ctk_label_set_text (CTK_LABEL (task->label), text);
      if (vnck_task_get_needs_attention (task))
        {
          _make_ctk_label_bold ((CTK_LABEL (task->label)));
          vnck_task_queue_glow (task);
        }
      else
        {
          _make_ctk_label_normal ((CTK_LABEL (task->label)));
          vnck_task_reset_glow (task);
        }
      g_free (text);
    }

  text = vnck_task_get_text (task, FALSE, FALSE);
  /* if text is NULL, this unsets the tooltip, which is probably what we'd want
   * to do */
  ctk_widget_set_tooltip_text (task->button, text);
  g_free (text);

  ctk_widget_queue_resize (CTK_WIDGET (task->tasklist));
}

static void
vnck_task_state_changed (VnckWindow     *window,
			 VnckWindowState changed_mask,
			 VnckWindowState new_state G_GNUC_UNUSED,
			 gpointer        data)
{
  VnckTasklist *tasklist = VNCK_TASKLIST (data);

  if (changed_mask & VNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      vnck_tasklist_update_lists  (tasklist);
      ctk_widget_queue_resize (CTK_WIDGET (tasklist));
      return;
    }

  if ((changed_mask & VNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & VNCK_WINDOW_STATE_URGENT))
    {
      VnckWorkspace *active_workspace =
        vnck_screen_get_active_workspace (tasklist->priv->screen);

      if (active_workspace                              &&
          (active_workspace != vnck_window_get_workspace (window) ||
	   (vnck_workspace_is_virtual (active_workspace) &&
	    !vnck_window_is_in_viewport (window, active_workspace))))
        {
          vnck_tasklist_update_lists (tasklist);
          ctk_widget_queue_resize (CTK_WIDGET (tasklist));
        }
    }

  if ((changed_mask & VNCK_WINDOW_STATE_MINIMIZED)         ||
      (changed_mask & VNCK_WINDOW_STATE_DEMANDS_ATTENTION) ||
      (changed_mask & VNCK_WINDOW_STATE_URGENT))
    {
      VnckTask *win_task = NULL;

      /* FIXME: Handle group modal dialogs */
      for (; window && !win_task; window = vnck_window_get_transient (window))
        win_task = g_hash_table_lookup (tasklist->priv->win_hash, window);

      if (win_task)
	{
	  VnckTask *class_group_task;

	  vnck_task_update_visible_state (win_task);

	  class_group_task =
            g_hash_table_lookup (tasklist->priv->class_group_hash,
                                 win_task->class_group);

	  if (class_group_task)
	    vnck_task_update_visible_state (class_group_task);
	}
    }

}

static void
vnck_task_icon_changed (VnckWindow *window G_GNUC_UNUSED,
			gpointer    data)
{
  VnckTask *task = VNCK_TASK (data);

  if (task)
    vnck_task_update_visible_state (task);
}

static void
vnck_task_name_changed (VnckWindow *window G_GNUC_UNUSED,
			gpointer    data)
{
  VnckTask *task = VNCK_TASK (data);

  if (task)
    vnck_task_update_visible_state (task);
}

static void
vnck_task_class_name_changed (VnckClassGroup *class_group G_GNUC_UNUSED,
			      gpointer        data)
{
  VnckTask *task = VNCK_TASK (data);

  if (task)
    vnck_task_update_visible_state (task);
}

static void
vnck_task_class_icon_changed (VnckClassGroup *class_group G_GNUC_UNUSED,
			      gpointer        data)
{
  VnckTask *task = VNCK_TASK (data);

  if (task)
    vnck_task_update_visible_state (task);
}

static gboolean
vnck_task_motion_timeout (gpointer data)
{
  VnckWorkspace *ws;
  VnckTask *task = VNCK_TASK (data);

  task->button_activate = 0;

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  ws = vnck_window_get_workspace (task->window);
  if (ws && ws != vnck_screen_get_active_workspace (vnck_screen_get_default ()))
  {
    vnck_workspace_activate (ws, task->dnd_timestamp);
  }
  vnck_window_activate_transient (task->window, task->dnd_timestamp);

  task->dnd_timestamp = 0;

  return FALSE;
}

static void
vnck_task_drag_leave (CtkWidget      *widget,
		      CdkDragContext *context G_GNUC_UNUSED,
		      guint           time G_GNUC_UNUSED,
		      VnckTask       *task)
{
  if (task->button_activate != 0)
    {
      g_source_remove (task->button_activate);
      task->button_activate = 0;
    }

  ctk_drag_unhighlight (widget);
}

static gboolean
vnck_task_drag_motion (CtkWidget      *widget,
		       CdkDragContext *context,
		       gint            x G_GNUC_UNUSED,
		       gint            y G_GNUC_UNUSED,
		       guint           time,
		       VnckTask       *task)
{
  if (ctk_drag_dest_find_target (widget, context, NULL))
    {
      ctk_drag_highlight (widget);
      cdk_drag_status (context, cdk_drag_context_get_suggested_action (context), time);
    }
  else
    {
      task->dnd_timestamp = time;

      if (task->button_activate == 0 && task->type == VNCK_TASK_WINDOW)
        {
           task->button_activate = g_timeout_add_seconds (VNCK_ACTIVATE_TIMEOUT,
                                                          vnck_task_motion_timeout,
                                                          task);
        }

      cdk_drag_status (context, 0, time);
    }
  return TRUE;
}

static void
vnck_task_drag_begin (CtkWidget      *widget G_GNUC_UNUSED,
		      CdkDragContext *context,
		      VnckTask       *task)
{
  _vnck_window_set_as_drag_icon (task->window, context,
                                 CTK_WIDGET (task->tasklist));

  task->tasklist->priv->drag_start_time = ctk_get_current_event_time ();
}

static void
vnck_task_drag_end (CtkWidget      *widget G_GNUC_UNUSED,
		    CdkDragContext *context G_GNUC_UNUSED,
		    VnckTask       *task)
{
  task->tasklist->priv->drag_start_time = 0;
}

static void
vnck_task_drag_data_get (CtkWidget        *widget G_GNUC_UNUSED,
			 CdkDragContext   *context G_GNUC_UNUSED,
			 CtkSelectionData *selection_data,
			 guint             info G_GNUC_UNUSED,
			 guint             time G_GNUC_UNUSED,
			 VnckTask         *task)
{
  gulong xid;

  xid = vnck_window_get_xid (task->window);
  ctk_selection_data_set (selection_data,
                          ctk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static void
vnck_task_drag_data_received (CtkWidget        *widget G_GNUC_UNUSED,
			      CdkDragContext   *context,
			      gint              x G_GNUC_UNUSED,
			      gint              y G_GNUC_UNUSED,
			      CtkSelectionData *data,
			      guint             info G_GNUC_UNUSED,
			      guint             time,
			      VnckTask         *target_task)
{
  VnckTasklist *tasklist;
  GList        *l, *windows;
  VnckWindow   *window;
  gulong       *xid;
  guint         new_order, old_order, order;
  VnckWindow   *found_window;

  if ((ctk_selection_data_get_length (data) != sizeof (gulong)) ||
      (ctk_selection_data_get_format (data) != 8))
    {
      ctk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  tasklist = target_task->tasklist;
  xid = (gulong *) ctk_selection_data_get_data (data);
  found_window = NULL;
  new_order = 0;
  windows = vnck_screen_get_windows (tasklist->priv->screen);

  for (l = windows; l; l = l->next)
    {
       window = VNCK_WINDOW (l->data);
       if (vnck_window_get_xid (window) == *xid)
         {
            old_order = vnck_window_get_sort_order (window);
            new_order = vnck_window_get_sort_order (target_task->window);
            if (old_order < new_order)
              new_order++;
            found_window = window;
            break;
         }
    }

  if (target_task->window == found_window)
    {
      CtkSettings  *settings;
      guint         double_click_time;

      settings = ctk_settings_get_for_screen (ctk_widget_get_screen (CTK_WIDGET (tasklist)));
      double_click_time = 0;
      g_object_get (G_OBJECT (settings),
                    "ctk-double-click-time", &double_click_time,
                    NULL);

      if ((time - tasklist->priv->drag_start_time) < double_click_time)
        {
          vnck_tasklist_activate_task_window (target_task, time);
          ctk_drag_finish (context, TRUE, FALSE, time);
          return;
        }
    }

  if (found_window)
    {
       for (l = windows; l; l = l->next)
         {
            window = VNCK_WINDOW (l->data);
            order = vnck_window_get_sort_order (window);
            if (order >= new_order)
              vnck_window_set_sort_order (window, order + 1);
         }
       vnck_window_set_sort_order (found_window, new_order);

       if (!tasklist->priv->include_all_workspaces &&
           !vnck_window_is_pinned (found_window))
         {
           VnckWorkspace *active_space;
           active_space = vnck_screen_get_active_workspace (tasklist->priv->screen);
           vnck_window_move_to_workspace (found_window, active_space);
         }

       ctk_widget_queue_resize (CTK_WIDGET (tasklist));
    }

    ctk_drag_finish (context, TRUE, FALSE, time);
}

static gboolean
vnck_task_button_press_event (CtkWidget      *widget G_GNUC_UNUSED,
			      CdkEventButton *event,
			      gpointer        data)
{
  VnckTask *task = VNCK_TASK (data);

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      if (event->button == 2)
        vnck_tasklist_activate_next_in_class_group (task, event->time);
      else
        vnck_task_popup_menu (task,
                              event->button == 3);
      return TRUE;

    case VNCK_TASK_WINDOW:
      if (event->button == 1)
        {
          /* is_most_recently_activated == is_active for click &
	   * sloppy focus methods.  We use the former here because
	   * 'mouse' focus provides a special case.  In that case, no
	   * window will be active, but if a window was the most
	   * recently active one (i.e. user moves mouse straight from
	   * window to tasklist), then we should still minimize it.
           */
          if (vnck_window_is_most_recently_activated (task->window))
            task->was_active = TRUE;
          else
            task->was_active = FALSE;

          return FALSE;
        }
      else if (event->button == 2)
        {
          /* middle-click close window */
          if (task->tasklist->priv->middle_click_close == TRUE)
            {
              vnck_window_close (task->window, ctk_get_current_event_time ());
              return TRUE;
            }
        }
      else if (event->button == 3)
        {
          if (task->action_menu)
            ctk_widget_destroy (task->action_menu);

          g_assert (task->action_menu == NULL);

          task->action_menu = vnck_action_menu_new (task->window);

          g_object_add_weak_pointer (G_OBJECT (task->action_menu),
                                     (void**) &task->action_menu);

          ctk_menu_set_screen (CTK_MENU (task->action_menu),
                               _vnck_screen_get_cdk_screen (task->tasklist->priv->screen));

          ctk_widget_show (task->action_menu);
          ctk_menu_popup_at_widget (CTK_MENU (task->action_menu), task->button,
                                    CDK_GRAVITY_SOUTH_WEST,
                                    CDK_GRAVITY_NORTH_WEST,
                                    (CdkEvent *) event);

          g_signal_connect (task->action_menu, "selection-done",
                            G_CALLBACK (ctk_widget_destroy), NULL);

          return TRUE;
        }
      break;

    case VNCK_TASK_STARTUP_SEQUENCE:
    default:
      break;
    }

  return FALSE;
}

static GList*
vnck_task_extract_windows (VnckTask *task)
{
  GList *windows = NULL;
  GList *l;

  /* Add the ungrouped window in the task */
  if (task->window != NULL)
    {
      windows = g_list_prepend (windows, task->window);
    }

  /* Add any grouped windows available in the task */
  for (l = task->windows; l != NULL; l = l->next)
    {
      VnckTask *t = VNCK_TASK (l->data);
      windows = g_list_prepend (windows, t->window);
    }

  return g_list_reverse (windows);
}

static gboolean
vnck_task_enter_notify_event (CtkWidget *widget G_GNUC_UNUSED,
			      CdkEvent  *event G_GNUC_UNUSED,
			      gpointer   data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *windows = vnck_task_extract_windows (task);

  g_signal_emit (G_OBJECT (task->tasklist),
                 signals[TASK_ENTER_NOTIFY],
                 0, windows);

  g_list_free (windows);

  return FALSE;
}

static gboolean
vnck_task_leave_notify_event (CtkWidget *widget G_GNUC_UNUSED,
			      CdkEvent  *event G_GNUC_UNUSED,
			      gpointer   data)
{
  VnckTask *task = VNCK_TASK (data);
  GList *windows = vnck_task_extract_windows (task);

  g_signal_emit (G_OBJECT (task->tasklist),
                 signals[TASK_LEAVE_NOTIFY],
                 0, windows);

  g_list_free (windows);

  return FALSE;
}

static gboolean
vnck_task_scroll_event (CtkWidget *widget G_GNUC_UNUSED,
			CdkEvent  *event,
			gpointer   data)
{
  VnckTask *task = VNCK_TASK (data);

  return vnck_tasklist_scroll_event (CTK_WIDGET (task->tasklist), (CdkEventScroll *) event);
}

static gboolean
vnck_task_draw (CtkWidget *widget,
                cairo_t   *cr,
                gpointer   data);

static void
vnck_task_create_widgets (VnckTask *task, CtkReliefStyle relief)
{
  CtkWidget *hbox;
  GdkPixbuf *pixbuf;
  char *text;
  static const CtkTargetEntry targets[] = {
    { (gchar *) "application/x-vnck-window-id", 0, 0 }
  };

  if (task->type == VNCK_TASK_STARTUP_SEQUENCE)
    task->button = ctk_button_new ();
  else
    task->button = ctk_toggle_button_new ();

  ctk_button_set_relief (CTK_BUTTON (task->button), relief);

  task->button_activate = 0;
  g_object_add_weak_pointer (G_OBJECT (task->button),
                             (void**) &task->button);

  ctk_widget_set_name (task->button,
		       "tasklist-button");

  if (task->type == VNCK_TASK_WINDOW)
    {
      ctk_drag_source_set (CTK_WIDGET (task->button),
                           CDK_BUTTON1_MASK,
                           targets, 1,
                           CDK_ACTION_MOVE);
      ctk_drag_dest_set (CTK_WIDGET (task->button), CTK_DEST_DEFAULT_DROP,
                         targets, 1, CDK_ACTION_MOVE);
    }
  else
    ctk_drag_dest_set (CTK_WIDGET (task->button), 0,
                       NULL, 0, CDK_ACTION_DEFAULT);

  hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

  pixbuf = vnck_task_get_icon (task);
  if (pixbuf)
    {
      task->image = ctk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    task->image = ctk_image_new ();

  ctk_widget_show (task->image);

  text = vnck_task_get_text (task, TRUE, TRUE);
  task->label = ctk_label_new (text);
  ctk_label_set_xalign (CTK_LABEL (task->label), 0.0);
  ctk_label_set_ellipsize (CTK_LABEL (task->label),
                          PANGO_ELLIPSIZE_END);

  if (vnck_task_get_needs_attention (task))
    {
      _make_ctk_label_bold ((CTK_LABEL (task->label)));
      vnck_task_queue_glow (task);
    }

  ctk_widget_show (task->label);

  ctk_box_pack_start (CTK_BOX (hbox), task->image, FALSE, FALSE,
		      TASKLIST_BUTTON_PADDING);
  ctk_box_pack_start (CTK_BOX (hbox), task->label, TRUE, TRUE,
		      TASKLIST_BUTTON_PADDING);

  ctk_container_add (CTK_CONTAINER (task->button), hbox);
  ctk_widget_show (hbox);
  g_free (text);

  text = vnck_task_get_text (task, FALSE, FALSE);
  ctk_widget_set_tooltip_text (task->button, text);
  g_free (text);

  /* Set up signals */
  if (CTK_IS_TOGGLE_BUTTON (task->button))
    g_signal_connect_object (G_OBJECT (task->button), "toggled",
                             G_CALLBACK (vnck_task_button_toggled),
                             G_OBJECT (task),
                             0);

  g_signal_connect_object (G_OBJECT (task->button), "size_allocate",
                           G_CALLBACK (vnck_task_size_allocated),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT (task->button), "button_press_event",
                           G_CALLBACK (vnck_task_button_press_event),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT (task->button), "enter_notify_event",
                           G_CALLBACK (vnck_task_enter_notify_event),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT (task->button), "leave_notify_event",
                           G_CALLBACK (vnck_task_leave_notify_event),
                           G_OBJECT (task),
                           0);

  ctk_widget_add_events (task->button, CDK_SCROLL_MASK);
  g_signal_connect_object (G_OBJECT (task->button), "scroll_event",
                           G_CALLBACK (vnck_task_scroll_event),
                           G_OBJECT (task),
                           0);

  g_signal_connect_object (G_OBJECT(task->button), "drag_motion",
                           G_CALLBACK (vnck_task_drag_motion),
                           G_OBJECT (task),
                           0);

  if (task->type == VNCK_TASK_WINDOW)
    {
      g_signal_connect_object (G_OBJECT (task->button), "drag_data_received",
                               G_CALLBACK (vnck_task_drag_data_received),
                               G_OBJECT (task),
                               0);

    }

  g_signal_connect_object (G_OBJECT(task->button), "drag_leave",
                           G_CALLBACK (vnck_task_drag_leave),
                           G_OBJECT (task),
                           0);

  if (task->type == VNCK_TASK_WINDOW) {
      g_signal_connect_object (G_OBJECT(task->button), "drag_data_get",
                               G_CALLBACK (vnck_task_drag_data_get),
                               G_OBJECT (task),
                               0);

      g_signal_connect_object (G_OBJECT(task->button), "drag_begin",
                               G_CALLBACK (vnck_task_drag_begin),
                               G_OBJECT (task),
                               0);

      g_signal_connect_object (G_OBJECT(task->button), "drag_end",
                               G_CALLBACK (vnck_task_drag_end),
                               G_OBJECT (task),
                               0);
  }

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      task->class_name_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "name_changed",
						       G_CALLBACK (vnck_task_class_name_changed), task);
      task->class_icon_changed_tag = g_signal_connect (G_OBJECT (task->class_group), "icon_changed",
						       G_CALLBACK (vnck_task_class_icon_changed), task);
      break;

    case VNCK_TASK_WINDOW:
      task->state_changed_tag = g_signal_connect (G_OBJECT (task->window), "state_changed",
                                                  G_CALLBACK (vnck_task_state_changed), task->tasklist);
      task->icon_changed_tag = g_signal_connect (G_OBJECT (task->window), "icon_changed",
                                                 G_CALLBACK (vnck_task_icon_changed), task);
      task->name_changed_tag = g_signal_connect (G_OBJECT (task->window), "name_changed",
						 G_CALLBACK (vnck_task_name_changed), task);
      break;

    case VNCK_TASK_STARTUP_SEQUENCE:
      break;

    default:
      g_assert_not_reached ();
    }

  g_signal_connect_object (task->button, "draw",
                           G_CALLBACK (vnck_task_draw),
                           G_OBJECT (task),
                           G_CONNECT_AFTER);
}

#define ARROW_SPACE 4
#define ARROW_SIZE 12
#define INDICATOR_SIZE 7

static gboolean
vnck_task_draw (CtkWidget *widget,
                cairo_t   *cr,
                gpointer   data)
{
  int x, y;
  VnckTask *task;
  CtkStyleContext *context;
  CtkStateFlags state;
  CtkBorder padding;
  CtkWidget    *tasklist_widget;
  gint width, height;
  gboolean overlay_rect;
  gint arrow_width;
  gint arrow_height;
  CdkRGBA color;

  task = VNCK_TASK (data);

  switch (task->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      context = ctk_widget_get_style_context (widget);

      state = ctk_style_context_get_state (context);
      ctk_style_context_get_padding (context, state, &padding);

      state = (task->tasklist->priv->active_class_group == task) ?
              CTK_STATE_FLAG_ACTIVE : CTK_STATE_FLAG_NORMAL;

      ctk_style_context_save (context);
      ctk_style_context_set_state (context, state);
      ctk_style_context_get_color (context, state, &color);
      ctk_style_context_restore (context);

      x = ctk_widget_get_allocated_width (widget) -
          (ctk_container_get_border_width (CTK_CONTAINER (widget)) + padding.right + ARROW_SIZE);
      y = ctk_widget_get_allocated_height (widget) / 2;

      arrow_width = INDICATOR_SIZE + ((INDICATOR_SIZE % 2) - 1);
      arrow_height = arrow_width / 2 + 1;
      x += (ARROW_SIZE - arrow_width) / 2;
      y -= (2 * arrow_height + ARROW_SPACE) / 2;

      cairo_save (cr);
      cdk_cairo_set_source_rgba (cr, &color);

      /* Up arrow */
      cairo_move_to (cr, x, y + arrow_height);
      cairo_line_to (cr, x + arrow_width / 2., y);
      cairo_line_to (cr, x + arrow_width, y + arrow_height);
      cairo_close_path (cr);
      cairo_fill (cr);

      /* Down arrow */
      y += arrow_height + ARROW_SPACE;
      cairo_move_to (cr, x, y);
      cairo_line_to (cr, x + arrow_width, y);
      cairo_line_to (cr, x + arrow_width / 2., y + arrow_height);
      cairo_close_path (cr);
      cairo_fill (cr);

      cairo_restore (cr);

      break;

    case VNCK_TASK_WINDOW:
    case VNCK_TASK_STARTUP_SEQUENCE:
    default:
      break;
    }

  if (task->glow_factor == 0.0)
    return FALSE;

  /* push a translucent overlay to paint to, so we can blend later */
  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);

  width = ctk_widget_get_allocated_width (task->button);
  height = ctk_widget_get_allocated_height (task->button);

  tasklist_widget = CTK_WIDGET (task->tasklist);

  context = ctk_widget_get_style_context (task->button);

  /* first draw the button */
  ctk_widget_style_get (tasklist_widget, "fade-overlay-rect", &overlay_rect, NULL);
  if (overlay_rect)
    {
      ctk_style_context_save (context);
      ctk_style_context_set_state (context, CTK_STATE_FLAG_SELECTED);

      /* Draw a rectangle with selected background color */
      ctk_render_background (context, cr, 0, 0, width, height);

      ctk_style_context_restore (context);
    }
  else
    {
      ctk_style_context_save (context);
      ctk_style_context_set_state (context, CTK_STATE_FLAG_SELECTED);
      ctk_style_context_add_class (context, CTK_STYLE_CLASS_BUTTON);

      cairo_save (cr);
      ctk_render_background (context, cr, 0, 0, width, height);
      ctk_render_frame (context, cr, 0, 0, width, height);
      cairo_restore (cr);

      ctk_style_context_restore (context);
    }

  /* then the contents */
  ctk_container_propagate_draw (CTK_CONTAINER (task->button),
                                ctk_bin_get_child (CTK_BIN (task->button)),
                                cr);
  /* finally blend it */
  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, task->glow_factor);

  return FALSE;
}

static gint
vnck_task_compare_alphabetically (gconstpointer a,
                                  gconstpointer b)
{
  char *text1;
  char *text2;
  gint  result;

  text1 = vnck_task_get_text (VNCK_TASK (a), TRUE, FALSE);
  text2 = vnck_task_get_text (VNCK_TASK (b), TRUE, FALSE);

  result= g_utf8_collate (text1, text2);

  g_free (text1);
  g_free (text2);

  return result;
}

static gint
compare_class_group_tasks (VnckTask *task1, VnckTask *task2)
{
  const char *name1, *name2;

  name1 = vnck_class_group_get_name (task1->class_group);
  name2 = vnck_class_group_get_name (task2->class_group);

  return g_utf8_collate (name1, name2);
}

static gint
vnck_task_compare (gconstpointer  a,
		   gconstpointer  b)
{
  VnckTask *task1 = VNCK_TASK (a);
  VnckTask *task2 = VNCK_TASK (b);
  gint pos1, pos2;

  pos1 = pos2 = 0;  /* silence the compiler */

  switch (task1->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      if (task2->type == VNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return -1; /* Sort groups before everything else */

    case VNCK_TASK_WINDOW:
      pos1 = vnck_window_get_sort_order (task1->window);
      break;
    case VNCK_TASK_STARTUP_SEQUENCE:
      pos1 = G_MAXINT; /* startup sequences are sorted at the end. */
      break;           /* Changing this will break scrolling.      */

    default:
      break;
    }

  switch (task2->type)
    {
    case VNCK_TASK_CLASS_GROUP:
      if (task1->type == VNCK_TASK_CLASS_GROUP)
	return compare_class_group_tasks (task1, task2);
      else
	return 1; /* Sort groups before everything else */

    case VNCK_TASK_WINDOW:
      pos2 = vnck_window_get_sort_order (task2->window);
      break;
    case VNCK_TASK_STARTUP_SEQUENCE:
      pos2 = G_MAXINT;
      break;

    default:
      break;
    }

  if (pos1 < pos2)
    return -1;
  else if (pos1 > pos2)
    return 1;
  else
    return 0; /* should only happen if there's multiple processes being
               * started, and then who cares about sort order... */
}

static void
remove_startup_sequences_for_window (VnckTasklist *tasklist,
                                     VnckWindow   *window)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  const char *win_id;
  GList *tmp;

  win_id = _vnck_window_get_startup_id (window);
  if (win_id == NULL)
    return;

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      VnckTask *task = tmp->data;
      GList *next = tmp->next;
      const char *task_id;

      g_assert (task->type == VNCK_TASK_STARTUP_SEQUENCE);

      task_id = sn_startup_sequence_get_id (task->startup_sequence);

      if (task_id && strcmp (task_id, win_id) == 0)
        ctk_widget_destroy (task->button);

      tmp = next;
    }
#else
  ; /* nothing */
#endif
}

static VnckTask *
vnck_task_new_from_window (VnckTasklist *tasklist,
			   VnckWindow   *window)
{
  VnckTask *task;

  task = g_object_new (VNCK_TYPE_TASK, NULL);
  task->type = VNCK_TASK_WINDOW;
  task->window = g_object_ref (window);
  task->class_group = g_object_ref (vnck_window_get_class_group (window));
  task->tasklist = tasklist;

  vnck_task_create_widgets (task, tasklist->priv->relief);

  remove_startup_sequences_for_window (tasklist, window);

  return task;
}

static VnckTask *
vnck_task_new_from_class_group (VnckTasklist   *tasklist,
				VnckClassGroup *class_group)
{
  VnckTask *task;

  task = g_object_new (VNCK_TYPE_TASK, NULL);

  task->type = VNCK_TASK_CLASS_GROUP;
  task->window = NULL;
  task->class_group = g_object_ref (class_group);
  task->tasklist = tasklist;

  vnck_task_create_widgets (task, tasklist->priv->relief);

  return task;
}

#ifdef HAVE_STARTUP_NOTIFICATION
static VnckTask*
vnck_task_new_from_startup_sequence (VnckTasklist      *tasklist,
                                     SnStartupSequence *sequence)
{
  VnckTask *task;

  task = g_object_new (VNCK_TYPE_TASK, NULL);

  task->type = VNCK_TASK_STARTUP_SEQUENCE;
  task->window = NULL;
  task->class_group = NULL;
  task->startup_sequence = sequence;
  sn_startup_sequence_ref (task->startup_sequence);
  task->tasklist = tasklist;

  vnck_task_create_widgets (task, tasklist->priv->relief);

  return task;
}

/* This should be fairly long, as it should never be required unless
 * apps or .desktop files are buggy, and it's confusing if
 * OpenOffice or whatever seems to stop launching - people
 * might decide they need to launch it again.
 */
#define STARTUP_TIMEOUT 15000

static gboolean
sequence_timeout_callback (void *user_data)
{
  VnckTasklist *tasklist = user_data;
  GList *tmp;
  gint64 now;
  long tv_sec, tv_usec;
  double elapsed;

  now = g_get_real_time ();

 restart:
  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);

      sn_startup_sequence_get_last_active_time (task->startup_sequence,
                                                &tv_sec, &tv_usec);

      elapsed = (now - (tv_sec * G_USEC_PER_SEC + tv_usec)) / 1000.0;

      if (elapsed > STARTUP_TIMEOUT)
        {
          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          ctk_widget_destroy (task->button);

          goto restart; /* don't iterate over changed list, just restart;
                         * not efficient but who cares here.
                         */
        }

      tmp = tmp->next;
    }

  if (tasklist->priv->startup_sequences == NULL)
    {
      tasklist->priv->startup_sequence_timeout = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static void
vnck_tasklist_sn_event (SnMonitorEvent *event,
                        void           *user_data)
{
  VnckTasklist *tasklist;

  tasklist = VNCK_TASKLIST (user_data);

  switch (sn_monitor_event_get_type (event))
    {
    case SN_MONITOR_EVENT_INITIATED:
      {
        VnckTask *task;

        task = vnck_task_new_from_startup_sequence (tasklist,
                                                    sn_monitor_event_get_startup_sequence (event));

        ctk_widget_set_parent (task->button, CTK_WIDGET (tasklist));
        ctk_widget_show (task->button);

        tasklist->priv->startup_sequences =
          g_list_prepend (tasklist->priv->startup_sequences,
                          task);

        if (tasklist->priv->startup_sequence_timeout == 0)
          {
            tasklist->priv->startup_sequence_timeout =
              g_timeout_add_seconds (1, sequence_timeout_callback,
                                     tasklist);
          }

        ctk_widget_queue_resize (CTK_WIDGET (tasklist));
      }
      break;

    case SN_MONITOR_EVENT_COMPLETED:
      {
        GList *tmp;
        tmp = tasklist->priv->startup_sequences;
        while (tmp != NULL)
          {
            VnckTask *task = VNCK_TASK (tmp->data);

            if (task->startup_sequence ==
                sn_monitor_event_get_startup_sequence (event))
              {
                g_assert (task->button != NULL);
                /* removes task from list as a side effect */
                ctk_widget_destroy (task->button);
                break;
              }

            tmp = tmp->next;
          }
      }
      break;

    case SN_MONITOR_EVENT_CHANGED:
      break;

    case SN_MONITOR_EVENT_CANCELED:
      break;

    default:
      break;
    }

  if (tasklist->priv->startup_sequences == NULL &&
      tasklist->priv->startup_sequence_timeout != 0)
    {
      g_source_remove (tasklist->priv->startup_sequence_timeout);
      tasklist->priv->startup_sequence_timeout = 0;
    }
}

static void
vnck_tasklist_check_end_sequence (VnckTasklist   *tasklist,
                                  VnckWindow     *window)
{
  const char *res_class;
  const char *res_name;
  GList *tmp;

  if (tasklist->priv->startup_sequences == NULL)
    return;

  res_class = vnck_window_get_class_group_name (window);
  res_name = vnck_window_get_class_instance_name (window);

  if (res_class == NULL && res_name == NULL)
    return;

  tmp = tasklist->priv->startup_sequences;
  while (tmp != NULL)
    {
      VnckTask *task = VNCK_TASK (tmp->data);
      const char *wmclass;

      wmclass = sn_startup_sequence_get_wmclass (task->startup_sequence);

      if (wmclass != NULL &&
          ((res_class && strcmp (res_class, wmclass) == 0) ||
           (res_name && strcmp (res_name, wmclass) == 0)))
        {
          sn_startup_sequence_complete (task->startup_sequence);

          g_assert (task->button != NULL);
          /* removes task from list as a side effect */
          ctk_widget_destroy (task->button);

          /* only match one */
          return;
        }

      tmp = tmp->next;
    }
}

#endif /* HAVE_STARTUP_NOTIFICATION */
