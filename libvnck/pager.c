/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* vim: set sw=2 et: */
/* pager object */

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
#include <glib/gi18n-lib.h>

#include "pager.h"
#include "workspace.h"
#include "window.h"
#include "xutils.h"
#include "pager-accessible-factory.h"
#include "workspace-accessible-factory.h"
#include "private.h"

/**
 * SECTION:pager
 * @short_description: a pager widget, showing the content of workspaces.
 * @see_also: #VnckScreen
 * @stability: Unstable
 *
 * A #VnckPager shows a miniature view of the workspaces, representing managed
 * windows by small rectangles, and allows the user to initiate various window
 * manager actions by manipulating these representations. The #VnckPager offers
 * ways to move windows between workspaces and to change the current workspace.
 *
 * Alternatively, a #VnckPager can be configured to only show the names of the
 * workspace instead of their contents.
 *
 * The #VnckPager is also responsible for setting the layout of the workspaces.
 * Since only one application can be responsible for setting the layout on a
 * screen, the #VnckPager automatically tries to obtain the manager selection
 * for the screen and only sets the layout if it owns the manager selection.
 * See vnck_pager_set_orientation() and vnck_pager_set_n_rows() to change the
 * layout.
 */

#define N_SCREEN_CONNECTIONS 11

struct _VnckPagerPrivate
{
  VnckScreen *screen;

  int n_rows; /* really columns for vertical orientation */
  VnckPagerDisplayMode display_mode;
  VnckPagerScrollMode scroll_mode;
  gboolean show_all_workspaces;
  CtkShadowType shadow_type;
  gboolean wrap_on_scroll;

  CtkOrientation orientation;
  int workspace_size;
  guint screen_connections[N_SCREEN_CONNECTIONS];
  int prelight; /* workspace mouse is hovering over */
  gboolean prelight_dnd; /* is dnd happening? */

  guint dragging :1;
  int drag_start_x;
  int drag_start_y;
  VnckWindow *drag_window;

  GdkPixbuf *bg_cache;

  int layout_manager_token;

  guint dnd_activate; /* GSource that triggers switching to this workspace during dnd */
  guint dnd_time; /* time of last event during dnd (for delayed workspace activation) */
};

G_DEFINE_TYPE_WITH_PRIVATE (VnckPager, vnck_pager, CTK_TYPE_WIDGET);

enum
{
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

static void vnck_pager_finalize    (GObject        *object);

static void     vnck_pager_realize       (CtkWidget        *widget);
static void     vnck_pager_unrealize     (CtkWidget        *widget);
static CtkSizeRequestMode vnck_pager_get_request_mode (CtkWidget *widget);
static void     vnck_pager_get_preferred_width (CtkWidget *widget,
                                                int       *minimum_width,
                                                int       *natural_width);
static void     vnck_pager_get_preferred_width_for_height (CtkWidget *widget,
                                                           int        height,
                                                           int       *minimum_width,
                                                           int       *natural_width);
static void     vnck_pager_get_preferred_height (CtkWidget *widget,
                                                 int       *minimum_height,
                                                 int       *natural_height);
static void     vnck_pager_get_preferred_height_for_width (CtkWidget *widget,
                                                           int        width,
                                                           int       *minimum_height,
                                                           int       *natural_height);
static void     vnck_pager_size_allocate (CtkWidget        *widget,
                                          CtkAllocation    *allocation);
static gboolean vnck_pager_draw          (CtkWidget        *widget,
                                          cairo_t          *cr);
static gboolean vnck_pager_button_press  (CtkWidget        *widget,
                                          CdkEventButton   *event);
static gboolean vnck_pager_drag_motion   (CtkWidget        *widget,
                                          CdkDragContext   *context,
                                          gint              x,
                                          gint              y,
                                          guint             time);
static void vnck_pager_drag_motion_leave (CtkWidget        *widget,
                                          CdkDragContext   *context,
                                          guint             time);
static gboolean vnck_pager_drag_drop	 (CtkWidget        *widget,
					  CdkDragContext   *context,
					  gint              x,
					  gint              y,
					  guint             time);
static void vnck_pager_drag_data_received (CtkWidget          *widget,
			  	           CdkDragContext     *context,
				           gint                x,
				           gint                y,
				           CtkSelectionData   *selection_data,
				           guint               info,
				           guint               time_);
static void vnck_pager_drag_data_get      (CtkWidget        *widget,
		                           CdkDragContext   *context,
		                           CtkSelectionData *selection_data,
		                           guint             info,
		                           guint             time);
static void vnck_pager_drag_end		  (CtkWidget        *widget,
					   CdkDragContext   *context);
static gboolean vnck_pager_motion        (CtkWidget        *widget,
                                          CdkEventMotion   *event);
static gboolean vnck_pager_leave_notify	 (CtkWidget          *widget,
					  CdkEventCrossing   *event);
static gboolean vnck_pager_button_release (CtkWidget        *widget,
                                           CdkEventButton   *event);
static gboolean vnck_pager_scroll_event  (CtkWidget        *widget,
                                          CdkEventScroll   *event);
static gboolean vnck_pager_query_tooltip (CtkWidget  *widget,
                                          gint        x,
                                          gint        y,
                                          gboolean    keyboard_tip,
                                          CtkTooltip *tooltip);
static void workspace_name_changed_callback (VnckWorkspace *workspace,
                                             gpointer       data);

static gboolean vnck_pager_window_state_is_relevant (int state);
static gint vnck_pager_window_get_workspace   (VnckWindow  *window,
                                               gboolean     is_state_relevant);
static void vnck_pager_queue_draw_workspace   (VnckPager   *pager,
					       gint	    i);
static void vnck_pager_queue_draw_window (VnckPager	   *pager,
					  VnckWindow	   *window);

static void vnck_pager_connect_screen    (VnckPager  *pager);
static void vnck_pager_connect_window    (VnckPager  *pager,
                                          VnckWindow *window);
static void vnck_pager_disconnect_screen (VnckPager  *pager);
static void vnck_pager_disconnect_window (VnckPager  *pager,
                                          VnckWindow *window);

static gboolean vnck_pager_set_layout_hint (VnckPager  *pager);

static void vnck_pager_clear_drag (VnckPager *pager);
static void vnck_pager_check_prelight (VnckPager *pager,
				       gint	  x,
				       gint	  y,
				       gboolean	  dnd);

static GdkPixbuf* vnck_pager_get_background (VnckPager *pager,
                                             int        width,
                                             int        height);

static AtkObject* vnck_pager_get_accessible (CtkWidget *widget);


static void
vnck_pager_init (VnckPager *pager)
{
  int i;
  static const CtkTargetEntry targets[] = {
    { (gchar *) "application/x-vnck-window-id", 0, 0}
  };

  pager->priv = vnck_pager_get_instance_private (pager);

  pager->priv->n_rows = 1;
  pager->priv->display_mode = VNCK_PAGER_DISPLAY_CONTENT;
  pager->priv->scroll_mode = VNCK_PAGER_SCROLL_2D;
  pager->priv->show_all_workspaces = TRUE;
  pager->priv->shadow_type = CTK_SHADOW_NONE;
  pager->priv->wrap_on_scroll = FALSE;

  pager->priv->orientation = CTK_ORIENTATION_HORIZONTAL;
  pager->priv->workspace_size = 48;

  for (i = 0; i < N_SCREEN_CONNECTIONS; i++)
    pager->priv->screen_connections[i] = 0;

  pager->priv->prelight = -1;

  pager->priv->layout_manager_token = VNCK_NO_MANAGER_TOKEN;

  g_object_set (pager, "has-tooltip", TRUE, NULL);

  ctk_drag_dest_set (CTK_WIDGET (pager), 0, targets, G_N_ELEMENTS (targets), CDK_ACTION_MOVE);
  ctk_widget_set_can_focus (CTK_WIDGET (pager), TRUE);
}

static void
vnck_pager_class_init (VnckPagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

  object_class->finalize = vnck_pager_finalize;

  widget_class->realize = vnck_pager_realize;
  widget_class->unrealize = vnck_pager_unrealize;
  widget_class->get_request_mode = vnck_pager_get_request_mode;
  widget_class->get_preferred_width = vnck_pager_get_preferred_width;
  widget_class->get_preferred_width_for_height = vnck_pager_get_preferred_width_for_height;
  widget_class->get_preferred_height = vnck_pager_get_preferred_height;
  widget_class->get_preferred_height_for_width = vnck_pager_get_preferred_height_for_width;
  widget_class->size_allocate = vnck_pager_size_allocate;
  widget_class->draw = vnck_pager_draw;
  widget_class->button_press_event = vnck_pager_button_press;
  widget_class->button_release_event = vnck_pager_button_release;
  widget_class->scroll_event = vnck_pager_scroll_event;
  widget_class->motion_notify_event = vnck_pager_motion;
  widget_class->leave_notify_event = vnck_pager_leave_notify;
  widget_class->get_accessible = vnck_pager_get_accessible;
  widget_class->drag_leave = vnck_pager_drag_motion_leave;
  widget_class->drag_motion = vnck_pager_drag_motion;
  widget_class->drag_drop = vnck_pager_drag_drop;
  widget_class->drag_data_received = vnck_pager_drag_data_received;
  widget_class->drag_data_get = vnck_pager_drag_data_get;
  widget_class->drag_end = vnck_pager_drag_end;
  widget_class->query_tooltip = vnck_pager_query_tooltip;

  ctk_widget_class_set_css_name (widget_class, "vnck-pager");
}

static void
vnck_pager_finalize (GObject *object)
{
  VnckPager *pager;

  pager = VNCK_PAGER (object);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  if (pager->priv->dnd_activate != 0)
    {
      g_source_remove (pager->priv->dnd_activate);
      pager->priv->dnd_activate = 0;
    }

  G_OBJECT_CLASS (vnck_pager_parent_class)->finalize (object);
}

static void
_vnck_pager_set_screen (VnckPager *pager)
{
  CdkScreen *cdkscreen;

  if (!ctk_widget_has_screen (CTK_WIDGET (pager)))
    return;

  cdkscreen = ctk_widget_get_screen (CTK_WIDGET (pager));
  pager->priv->screen = vnck_screen_get (cdk_x11_screen_get_screen_number (cdkscreen));

  if (!vnck_pager_set_layout_hint (pager))
    {
      _VnckLayoutOrientation orientation;

      /* we couldn't set the layout on the screen. This means someone else owns
       * it. Let's at least show the correct layout. */
      _vnck_screen_get_workspace_layout (pager->priv->screen,
                                         &orientation,
                                         &pager->priv->n_rows,
                                         NULL, NULL);

      /* test in this order to default to horizontal in case there was in issue
       * when fetching the layout */
      if (orientation == VNCK_LAYOUT_ORIENTATION_VERTICAL)
        pager->priv->orientation = CTK_ORIENTATION_VERTICAL;
      else
        pager->priv->orientation = CTK_ORIENTATION_HORIZONTAL;

      ctk_widget_queue_resize (CTK_WIDGET (pager));
    }

  vnck_pager_connect_screen (pager);
}

static void
vnck_pager_realize (CtkWidget *widget)
{

  CdkWindowAttr attributes;
  gint attributes_mask;
  VnckPager *pager;
  CtkAllocation allocation;
  CdkWindow *window;

  pager = VNCK_PAGER (widget);

  /* do not call the parent class realize since we're doing things a bit
   * differently here */
  ctk_widget_set_realized (widget, TRUE);

  ctk_widget_get_allocation (widget, &allocation);

  attributes.window_type = CDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = CDK_INPUT_OUTPUT;
  attributes.visual = ctk_widget_get_visual (widget);
  attributes.event_mask = ctk_widget_get_events (widget) | CDK_EXPOSURE_MASK |
	  		  CDK_BUTTON_PRESS_MASK | CDK_BUTTON_RELEASE_MASK |
			  CDK_SCROLL_MASK |
			  CDK_LEAVE_NOTIFY_MASK | CDK_POINTER_MOTION_MASK |
			  CDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = CDK_WA_X | CDK_WA_Y | CDK_WA_VISUAL;

  window = cdk_window_new (ctk_widget_get_parent_window (widget), &attributes, attributes_mask);
  ctk_widget_set_window (widget, window);
  cdk_window_set_user_data (window, widget);

  /* connect to the screen of this pager. In theory, this will already have
   * been done in vnck_pager_size_request() */
  if (pager->priv->screen == NULL)
    _vnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);
}

static void
vnck_pager_unrealize (CtkWidget *widget)
{
  VnckPager *pager;

  pager = VNCK_PAGER (widget);

  vnck_pager_clear_drag (pager);
  pager->priv->prelight = -1;
  pager->priv->prelight_dnd = FALSE;

  vnck_screen_release_workspace_layout (pager->priv->screen,
                                        pager->priv->layout_manager_token);
  pager->priv->layout_manager_token = VNCK_NO_MANAGER_TOKEN;

  vnck_pager_disconnect_screen (pager);
  pager->priv->screen = NULL;

  CTK_WIDGET_CLASS (vnck_pager_parent_class)->unrealize (widget);
}

static void
_vnck_pager_get_padding (VnckPager *pager,
                         CtkBorder *padding)
{
  if (pager->priv->shadow_type != CTK_SHADOW_NONE)
    {
      CtkWidget       *widget;
      CtkStyleContext *context;
      CtkStateFlags    state;

      widget = CTK_WIDGET (pager);
      context = ctk_widget_get_style_context (widget);
      state = ctk_style_context_get_state (context);
      ctk_style_context_get_padding (context, state, padding);
    }
  else
    {
      CtkBorder empty_padding = { 0, 0, 0, 0 };
      *padding = empty_padding;
    }
}

static int
_vnck_pager_get_workspace_width_for_height (VnckPager *pager,
                                            int        workspace_height)
{
  int workspace_width = 0;

  if (pager->priv->display_mode == VNCK_PAGER_DISPLAY_CONTENT)
    {
      VnckWorkspace *space;
      double screen_aspect;

      space = vnck_screen_get_workspace (pager->priv->screen, 0);

      if (space) {
        screen_aspect =
              (double) vnck_workspace_get_width (space) /
              (double) vnck_workspace_get_height (space);
      } else {
        screen_aspect =
              (double) vnck_screen_get_width (pager->priv->screen) /
              (double) vnck_screen_get_height (pager->priv->screen);
      }

      workspace_width = screen_aspect * workspace_height;
    }
  else
    {
      PangoLayout *layout;
      VnckScreen *screen;
      int n_spaces;
      int i, w;

      layout = ctk_widget_create_pango_layout  (CTK_WIDGET (pager), NULL);
      screen = pager->priv->screen;
      n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);
      workspace_width = 1;

      for (i = 0; i < n_spaces; i++)
	{
	  pango_layout_set_text (layout,
				 vnck_workspace_get_name (vnck_screen_get_workspace (screen, i)),
				 -1);
	  pango_layout_get_pixel_size (layout, &w, NULL);
	  workspace_width = MAX (workspace_width, w);
	}

      g_object_unref (layout);

      workspace_width += 2;
    }

  return workspace_width;
}

static int
_vnck_pager_get_workspace_height_for_width (VnckPager *pager,
                                            int        workspace_width)
{
  int workspace_height = 0;
  VnckWorkspace *space;
  double screen_aspect;

  /* TODO: Handle VNCK_PAGER_DISPLAY_NAME for this case */

  space = vnck_screen_get_workspace (pager->priv->screen, 0);

  if (space) {
    screen_aspect =
	  (double) vnck_workspace_get_height (space) /
	  (double) vnck_workspace_get_width (space);
  } else {
    screen_aspect =
	  (double) vnck_screen_get_height (pager->priv->screen) /
	  (double) vnck_screen_get_width (pager->priv->screen);
  }

  workspace_height = screen_aspect * workspace_width;

  return workspace_height;
}

static void
vnck_pager_size_request  (CtkWidget      *widget,
                          CtkRequisition *requisition)
{
  VnckPager *pager;
  int n_spaces;
  int spaces_per_row;
  int workspace_width, workspace_height;
  int n_rows;
  CtkBorder padding;

  pager = VNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _vnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  if (pager->priv->orientation == CTK_ORIENTATION_VERTICAL)
    {
      workspace_width = pager->priv->workspace_size;
      workspace_height = _vnck_pager_get_workspace_height_for_width (pager,
                                                                     workspace_width);

      requisition->width = workspace_width * n_rows + (n_rows - 1);
      requisition->height = workspace_height * spaces_per_row  + (spaces_per_row - 1);
    }
  else
    {
      workspace_height = pager->priv->workspace_size;
      workspace_width = _vnck_pager_get_workspace_width_for_height (pager,
                                                                    workspace_height);

      requisition->width = workspace_width * spaces_per_row + (spaces_per_row - 1);
      requisition->height = workspace_height * n_rows + (n_rows - 1);
    }

  _vnck_pager_get_padding (pager, &padding);
  requisition->width += padding.left + padding.right;
  requisition->height += padding.top + padding.bottom;
}

static CtkSizeRequestMode
vnck_pager_get_request_mode (CtkWidget *widget)
{
  VnckPager *pager;

  pager = VNCK_PAGER (widget);

  if (pager->priv->orientation == CTK_ORIENTATION_VERTICAL)
    return CTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
  else
    return CTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
vnck_pager_get_preferred_width (CtkWidget *widget,
                                int       *minimum_width,
                                int       *natural_width)
{
  CtkRequisition req;

  vnck_pager_size_request (widget, &req);

  *minimum_width = *natural_width = req.width;
}

static void
vnck_pager_get_preferred_width_for_height (CtkWidget *widget,
                                           int        height,
                                           int       *minimum_width,
                                           int       *natural_width)
{
  VnckPager *pager;
  int n_spaces;
  int n_rows;
  int spaces_per_row;
  int workspace_width, workspace_height;
  CtkBorder padding;
  int width = 0;

  pager = VNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _vnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  _vnck_pager_get_padding (pager, &padding);
  height -= padding.top + padding.bottom;
  width += padding.left + padding.right;

  height -= (n_rows - 1);
  workspace_height = height / n_rows;

  workspace_width = _vnck_pager_get_workspace_width_for_height (pager,
                                                                workspace_height);

  width += workspace_width * spaces_per_row + (spaces_per_row - 1);
  *natural_width = *minimum_width = width;
}

static void
vnck_pager_get_preferred_height (CtkWidget *widget,
                                 int       *minimum_height,
                                 int       *natural_height)
{
  CtkRequisition req;

  vnck_pager_size_request (widget, &req);

  *minimum_height = *natural_height = req.height;
}

static void
vnck_pager_get_preferred_height_for_width (CtkWidget *widget,
                                           int        width,
                                           int       *minimum_height,
                                           int       *natural_height)
{
  VnckPager *pager;
  int n_spaces;
  int n_rows;
  int spaces_per_row;
  int workspace_width, workspace_height;
  CtkBorder padding;
  int height = 0;

  pager = VNCK_PAGER (widget);

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _vnck_pager_set_screen (pager);
  g_assert (pager->priv->screen != NULL);

  g_assert (pager->priv->n_rows > 0);

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);

  if (pager->priv->show_all_workspaces)
    {
      n_rows = pager->priv->n_rows;
      spaces_per_row = (n_spaces + n_rows - 1) / n_rows;
    }
  else
    {
      n_rows = 1;
      spaces_per_row = 1;
    }

  _vnck_pager_get_padding (pager, &padding);
  width -= padding.left + padding.right;
  height += padding.top + padding.bottom;

  width -= (n_rows - 1);
  workspace_width = width / n_rows;

  workspace_height = _vnck_pager_get_workspace_height_for_width (pager,
                                                                 workspace_width);

  height += workspace_height * spaces_per_row + (spaces_per_row - 1);
  *natural_height = *minimum_height = height;
}

static gboolean
_vnck_pager_queue_resize (gpointer data)
{
  ctk_widget_queue_resize (CTK_WIDGET (data));
  return FALSE;
}

static void
vnck_pager_size_allocate (CtkWidget      *widget,
                          CtkAllocation  *allocation)
{
  VnckPager *pager;
  int workspace_size;
  CtkBorder padding;
  int width;
  int height;

  pager = VNCK_PAGER (widget);

  width = allocation->width;
  height = allocation->height;

  _vnck_pager_get_padding (pager, &padding);
  width  -= padding.left + padding.right;
  height -= padding.top + padding.bottom;

  g_assert (pager->priv->n_rows > 0);

  if (pager->priv->orientation == CTK_ORIENTATION_VERTICAL)
    {
      if (pager->priv->show_all_workspaces)
	workspace_size = (width - (pager->priv->n_rows - 1))  / pager->priv->n_rows;
      else
	workspace_size = width;
    }
  else
    {
      if (pager->priv->show_all_workspaces)
	workspace_size = (height - (pager->priv->n_rows - 1))/ pager->priv->n_rows;
      else
	workspace_size = height;
    }

  workspace_size = MAX (workspace_size, 1);

  if (workspace_size != pager->priv->workspace_size)
    {
      pager->priv->workspace_size = workspace_size;
      g_idle_add (_vnck_pager_queue_resize, pager);
      return;
    }

  CTK_WIDGET_CLASS (vnck_pager_parent_class)->size_allocate (widget,
                                                             allocation);
}

static void
get_workspace_rect (VnckPager    *pager,
                    int           space,
                    CdkRectangle *rect)
{
  int hsize, vsize;
  int n_spaces;
  int spaces_per_row;
  CtkWidget *widget;
  int col, row;
  CtkAllocation allocation;
  CtkBorder padding;

  widget = CTK_WIDGET (pager);

  ctk_widget_get_allocation (widget, &allocation);

  if (allocation.x < 0 || allocation.y < 0 ||
      allocation.width < 0 || allocation.height < 0)
    {
      rect->x = 0;
      rect->y = 0;
      rect->width = 0;
      rect->height = 0;

      return;
    }

  _vnck_pager_get_padding (pager, &padding);

  if (!pager->priv->show_all_workspaces)
    {
      VnckWorkspace *active_space;

      active_space = vnck_screen_get_active_workspace (pager->priv->screen);

      if (active_space && space == vnck_workspace_get_number (active_space))
	{
	  rect->x = padding.left;
	  rect->y = padding.top;
	  rect->width = allocation.width - padding.left - padding.right;
	  rect->height = allocation.height - padding.top - padding.bottom;
	}
      else
	{
	  rect->x = 0;
	  rect->y = 0;
	  rect->width = 0;
	  rect->height = 0;
	}

      return;
    }

  hsize = allocation.width;
  vsize = allocation.height;

  if (pager->priv->shadow_type != CTK_SHADOW_NONE)
    {
      hsize -= padding.left + padding.right;
      vsize -= padding.top + padding.bottom;
    }

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);

  g_assert (pager->priv->n_rows > 0);
  spaces_per_row = (n_spaces + pager->priv->n_rows - 1) / pager->priv->n_rows;

  if (pager->priv->orientation == CTK_ORIENTATION_VERTICAL)
    {
      rect->width = (hsize - (pager->priv->n_rows - 1)) / pager->priv->n_rows;
      rect->height = (vsize - (spaces_per_row - 1)) / spaces_per_row;

      col = space / spaces_per_row;
      row = space % spaces_per_row;

      if (ctk_widget_get_direction (widget) == CTK_TEXT_DIR_RTL)
        col = pager->priv->n_rows - col - 1;

      rect->x = (rect->width + 1) * col;
      rect->y = (rect->height + 1) * row;

      if (col == pager->priv->n_rows - 1)
	rect->width = hsize - rect->x;

      if (row  == spaces_per_row - 1)
	rect->height = vsize - rect->y;
    }
  else
    {
      rect->width = (hsize - (spaces_per_row - 1)) / spaces_per_row;
      rect->height = (vsize - (pager->priv->n_rows - 1)) / pager->priv->n_rows;

      col = space % spaces_per_row;
      row = space / spaces_per_row;

      if (ctk_widget_get_direction (widget) == CTK_TEXT_DIR_RTL)
        col = spaces_per_row - col - 1;

      rect->x = (rect->width + 1) * col;
      rect->y = (rect->height + 1) * row;

      if (col == spaces_per_row - 1)
	rect->width = hsize - rect->x;

      if (row  == pager->priv->n_rows - 1)
	rect->height = vsize - rect->y;
    }

  if (pager->priv->shadow_type != CTK_SHADOW_NONE)
    {
      rect->x += padding.left;
      rect->y += padding.top;
    }
}

static gboolean
vnck_pager_window_state_is_relevant (int state)
{
  return (state & (VNCK_WINDOW_STATE_HIDDEN | VNCK_WINDOW_STATE_SKIP_PAGER)) ? FALSE : TRUE;
}

static gint
vnck_pager_window_get_workspace (VnckWindow *window,
                                 gboolean    is_state_relevant)
{
  gint state;
  VnckWorkspace *workspace;

  state = vnck_window_get_state (window);
  if (is_state_relevant && !vnck_pager_window_state_is_relevant (state))
    return -1;
  workspace = vnck_window_get_workspace (window);
  if (workspace == NULL && vnck_window_is_pinned (window))
    workspace = vnck_screen_get_active_workspace (vnck_window_get_screen (window));

  return workspace ? vnck_workspace_get_number (workspace) : -1;
}

static GList*
get_windows_for_workspace_in_bottom_to_top (VnckScreen    *screen,
                                            VnckWorkspace *workspace)
{
  GList *result;
  GList *windows;
  GList *tmp;
  int workspace_num;

  result = NULL;
  workspace_num = vnck_workspace_get_number (workspace);

  windows = vnck_screen_get_windows_stacked (screen);
  for (tmp = windows; tmp != NULL; tmp = tmp->next)
    {
      VnckWindow *win = VNCK_WINDOW (tmp->data);
      if (vnck_pager_window_get_workspace (win, TRUE) == workspace_num)
	result = g_list_prepend (result, win);
    }

  result = g_list_reverse (result);

  return result;
}

static void
get_window_rect (VnckWindow         *window,
                 const CdkRectangle *workspace_rect,
                 CdkRectangle       *rect)
{
  double width_ratio, height_ratio;
  int x, y, width, height;
  VnckWorkspace *workspace;
  CdkRectangle unclipped_win_rect;

  workspace = vnck_window_get_workspace (window);
  if (workspace == NULL)
    workspace = vnck_screen_get_active_workspace (vnck_window_get_screen (window));

  /* scale window down by same ratio we scaled workspace down */
  width_ratio = (double) workspace_rect->width / (double) vnck_workspace_get_width (workspace);
  height_ratio = (double) workspace_rect->height / (double) vnck_workspace_get_height (workspace);

  vnck_window_get_geometry (window, &x, &y, &width, &height);

  x += vnck_workspace_get_viewport_x (workspace);
  y += vnck_workspace_get_viewport_y (workspace);
  x = x * width_ratio + 0.5;
  y = y * height_ratio + 0.5;
  width = width * width_ratio + 0.5;
  height = height * height_ratio + 0.5;

  x += workspace_rect->x;
  y += workspace_rect->y;

  if (width < 3)
    width = 3;
  if (height < 3)
    height = 3;

  unclipped_win_rect.x = x;
  unclipped_win_rect.y = y;
  unclipped_win_rect.width = width;
  unclipped_win_rect.height = height;

  cdk_rectangle_intersect ((CdkRectangle *) workspace_rect, &unclipped_win_rect, rect);
}

static void
draw_window (cairo_t            *cr,
             CtkWidget          *widget,
             VnckWindow         *win,
             const CdkRectangle *winrect,
             CtkStateFlags       state,
             gboolean            translucent)
{
  CtkStyleContext *context;
  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
  gboolean is_active;
  CdkRGBA fg;
  gdouble translucency;

  context = ctk_widget_get_style_context (widget);

  is_active = vnck_window_is_active (win);
  translucency = translucent ? 0.4 : 1.0;

  ctk_style_context_save (context);
  ctk_style_context_set_state (context, state);

  cairo_push_group (cr);

  ctk_render_background (context, cr, winrect->x + 1, winrect->y + 1,
                         MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));

  if (is_active)
    {
      /* Sharpen the foreground color */
      cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.3f);
      cairo_rectangle (cr, winrect->x + 1, winrect->y + 1,
                       MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));
      cairo_fill (cr);
    }

  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, translucency);

  icon = vnck_window_get_icon (win);

  icon_w = icon_h = 0;

  if (icon)
    {
      icon_w = cdk_pixbuf_get_width (icon);
      icon_h = cdk_pixbuf_get_height (icon);

      /* If the icon is too big, fall back to mini icon.
       * We don't arbitrarily scale the icon, because it's
       * just too slow on my Athlon 850.
       */
      if (icon_w > (winrect->width - 2) ||
          icon_h > (winrect->height - 2))
        {
          icon = vnck_window_get_mini_icon (win);
          if (icon)
            {
              icon_w = cdk_pixbuf_get_width (icon);
              icon_h = cdk_pixbuf_get_height (icon);

              /* Give up. */
              if (icon_w > (winrect->width - 2) ||
                  icon_h > (winrect->height - 2))
                icon = NULL;
            }
        }
    }

  if (icon)
    {
      icon_x = winrect->x + (winrect->width - icon_w) / 2;
      icon_y = winrect->y + (winrect->height - icon_h) / 2;

      cairo_push_group (cr);
      ctk_render_icon (context, cr, icon, icon_x, icon_y);
      cairo_pop_group_to_source (cr);
      cairo_paint_with_alpha (cr, translucency);
    }

  cairo_push_group (cr);
  ctk_render_frame (context, cr, winrect->x + 0.5, winrect->y + 0.5,
                    MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, translucency);

  ctk_style_context_get_color (context, state, &fg);
  fg.alpha = translucency;
  cdk_cairo_set_source_rgba (cr, &fg);
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   winrect->x + 0.5, winrect->y + 0.5,
                   MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_stroke (cr);

  ctk_style_context_restore (context);
}

static VnckWindow *
window_at_point (VnckPager     *pager,
                 VnckWorkspace *space,
                 CdkRectangle  *space_rect,
                 int            x,
                 int            y)
{
  VnckWindow *window;
  GList *windows;
  GList *tmp;

  window = NULL;

  windows = get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
                                                        space);

  /* clicks on top windows first */
  windows = g_list_reverse (windows);

  for (tmp = windows; tmp != NULL; tmp = tmp->next)
    {
      VnckWindow *win = VNCK_WINDOW (tmp->data);
      CdkRectangle winrect;

      get_window_rect (win, space_rect, &winrect);

      if (POINT_IN_RECT (x, y, winrect))
        {
          /* vnck_window_activate (win); */
          window = win;
          break;
        }
    }

  g_list_free (windows);

  return window;
}

static int
workspace_at_point (VnckPager *pager,
                    int        x,
                    int        y,
                    int       *viewport_x,
                    int       *viewport_y)
{
  CtkWidget *widget;
  int i;
  int n_spaces;
  CtkAllocation allocation;
  CtkBorder padding;

  widget = CTK_WIDGET (pager);

  ctk_widget_get_allocation (widget, &allocation);

  _vnck_pager_get_padding (pager, &padding);

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);

  i = 0;
  while (i < n_spaces)
    {
      CdkRectangle rect;

      get_workspace_rect (pager, i, &rect);

      /* If workspace is on the edge, pretend points on the frame belong to the
       * workspace.
       * Else, pretend the right/bottom line separating two workspaces belong
       * to the workspace.
       */

      if (rect.x == padding.left)
        {
          rect.x = 0;
          rect.width += padding.left;
        }
      if (rect.y == padding.top)
        {
          rect.y = 0;
          rect.height += padding.top;
        }
      if (rect.y + rect.height == allocation.height - padding.bottom)
        {
          rect.height += padding.bottom;
        }
      else
        {
          rect.height += 1;
        }
      if (rect.x + rect.width == allocation.width - padding.right)
        {
          rect.width += padding.right;
        }
      else
        {
          rect.width += 1;
        }

      if (POINT_IN_RECT (x, y, rect))
        {
	  double width_ratio, height_ratio;
	  VnckWorkspace *space;

	  space = vnck_screen_get_workspace (pager->priv->screen, i);
          g_assert (space != NULL);

          /* Scale x, y mouse coords to corresponding screenwide viewport coords */

          width_ratio = (double) vnck_workspace_get_width (space) / (double) rect.width;
          height_ratio = (double) vnck_workspace_get_height (space) / (double) rect.height;

          if (viewport_x)
            *viewport_x = width_ratio * (x - rect.x);
          if (viewport_y)
            *viewport_y = height_ratio * (y - rect.y);

	  return i;
	}

      ++i;
    }

  return -1;
}

static void
draw_dark_rectangle (CtkStyleContext *context,
                     cairo_t *cr,
                     CtkStateFlags state,
                     int rx, int ry, int rw, int rh)
{
  ctk_style_context_save (context);

  ctk_style_context_set_state (context, state);

  cairo_push_group (cr);

  ctk_render_background (context, cr, rx, ry, rw, rh);
  cairo_set_source_rgba (cr, 0.0f, 0.0f, 0.0f, 0.3f);
  cairo_rectangle (cr, rx, ry, rw, rh);
  cairo_fill (cr);

  cairo_pop_group_to_source (cr);
  cairo_paint (cr);

  ctk_style_context_restore (context);
}

static void
vnck_pager_draw_workspace (VnckPager    *pager,
                           cairo_t      *cr,
                           int           workspace,
                           CdkRectangle *rect,
                           GdkPixbuf    *bg_pixbuf)
{
  GList *windows;
  GList *tmp;
  gboolean is_current;
  VnckWorkspace *space;
  CtkWidget *widget;
  CtkStateFlags state;
  CtkStyleContext *context;

  space = vnck_screen_get_workspace (pager->priv->screen, workspace);
  if (!space)
    return;

  widget = CTK_WIDGET (pager);
  is_current = (space == vnck_screen_get_active_workspace (pager->priv->screen));

  state = CTK_STATE_FLAG_NORMAL;
  if (is_current)
    state |= CTK_STATE_FLAG_SELECTED;
  else if (workspace == pager->priv->prelight)
    state |= CTK_STATE_FLAG_PRELIGHT;

  context = ctk_widget_get_style_context (widget);

  /* FIXME in names mode, should probably draw things like a button.
   */

  if (bg_pixbuf)
    {
      cdk_cairo_set_source_pixbuf (cr, bg_pixbuf, rect->x, rect->y);
      cairo_paint (cr);
    }
  else
    {
      if (!vnck_workspace_is_virtual (space))
        {
          draw_dark_rectangle (context, cr, state,
                               rect->x, rect->y, rect->width, rect->height);
        }
      else
        {
          //FIXME prelight for dnd in the viewport?
          int workspace_width, workspace_height;
          int screen_width, screen_height;
          double width_ratio, height_ratio;
          double vx, vy, vw, vh; /* viewport */

          workspace_width = vnck_workspace_get_width (space);
          workspace_height = vnck_workspace_get_height (space);
          screen_width = vnck_screen_get_width (pager->priv->screen);
          screen_height = vnck_screen_get_height (pager->priv->screen);

          if ((workspace_width % screen_width == 0) &&
              (workspace_height % screen_height == 0))
            {
              int i, j;
              int active_i, active_j;
              int horiz_views;
              int verti_views;

              horiz_views = workspace_width / screen_width;
              verti_views = workspace_height / screen_height;

              /* do not forget thin lines to delimit "workspaces" */
              width_ratio = (rect->width - (horiz_views - 1)) /
                            (double) workspace_width;
              height_ratio = (rect->height - (verti_views - 1)) /
                             (double) workspace_height;

              if (is_current)
                {
                  active_i =
                    vnck_workspace_get_viewport_x (space) / screen_width;
                  active_j =
                    vnck_workspace_get_viewport_y (space) / screen_height;
                }
              else
                {
                  active_i = -1;
                  active_j = -1;
                }

              for (i = 0; i < horiz_views; i++)
                {
                  /* "+ i" is for the thin lines */
                  vx = rect->x + (width_ratio * screen_width) * i + i;

                  if (i == horiz_views - 1)
                    vw = rect->width + rect->x - vx;
                  else
                    vw = width_ratio * screen_width;

                  vh = height_ratio * screen_height;

                  for (j = 0; j < verti_views; j++)
                    {
                      CtkStateFlags rec_state = CTK_STATE_FLAG_NORMAL;

                      /* "+ j" is for the thin lines */
                      vy = rect->y + (height_ratio * screen_height) * j + j;

                      if (j == verti_views - 1)
                        vh = rect->height + rect->y - vy;

                      if (active_i == i && active_j == j)
                        rec_state = CTK_STATE_FLAG_SELECTED;

                      draw_dark_rectangle (context, cr, rec_state, vx, vy, vw, vh);
                    }
                }
            }
          else
            {
              width_ratio = rect->width / (double) workspace_width;
              height_ratio = rect->height / (double) workspace_height;

              /* first draw non-active part of the viewport */
              draw_dark_rectangle (context, cr, CTK_STATE_FLAG_NORMAL,
                                   rect->x, rect->y, rect->width, rect->height);

              if (is_current)
                {
                  /* draw the active part of the viewport */
                  vx = rect->x +
                    width_ratio * vnck_workspace_get_viewport_x (space);
                  vy = rect->y +
                    height_ratio * vnck_workspace_get_viewport_y (space);
                  vw = width_ratio * screen_width;
                  vh = height_ratio * screen_height;

                  draw_dark_rectangle (context, cr, CTK_STATE_FLAG_SELECTED,
                                       vx, vy, vw, vh);
                }
            }
        }
    }

  if (pager->priv->display_mode == VNCK_PAGER_DISPLAY_CONTENT)
    {
      windows = get_windows_for_workspace_in_bottom_to_top (pager->priv->screen,
							    vnck_screen_get_workspace (pager->priv->screen,
										       workspace));

      tmp = windows;
      while (tmp != NULL)
        {
          VnckWindow *win = tmp->data;
          CdkRectangle winrect;
          gboolean translucent;

          get_window_rect (win, rect, &winrect);

          translucent = (win == pager->priv->drag_window) && pager->priv->dragging;
          draw_window (cr, widget, win, &winrect, state, translucent);

          tmp = tmp->next;
        }

      g_list_free (windows);
    }
  else
    {
      /* Workspace name mode */
      CtkStateFlags layout_state;
      const char *workspace_name;
      PangoLayout *layout;
      VnckWorkspace *ws;
      int w, h;

      ws = vnck_screen_get_workspace (pager->priv->screen, workspace);
      workspace_name = vnck_workspace_get_name (ws);
      layout = ctk_widget_create_pango_layout (widget, workspace_name);

      pango_layout_get_pixel_size (layout, &w, &h);

      layout_state = (is_current) ? CTK_STATE_FLAG_SELECTED : CTK_STATE_FLAG_NORMAL;
      ctk_style_context_save (context);
      ctk_style_context_set_state (context, layout_state);
      ctk_render_layout (context, cr, rect->x + (rect->width - w) / 2,
                         rect->y + (rect->height - h) / 2, layout);

      ctk_style_context_restore (context);
      g_object_unref (layout);
    }

  if (workspace == pager->priv->prelight && pager->priv->prelight_dnd)
    {
      ctk_style_context_save (context);
      ctk_style_context_set_state (context, CTK_STATE_FLAG_NORMAL);
      ctk_render_frame (context, cr, rect->x, rect->y, rect->width, rect->height);
      ctk_style_context_restore (context);

      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
      cairo_set_line_width (cr, 1.0);
      cairo_rectangle (cr, rect->x + 0.5, rect->y + 0.5,
                           MAX (0, rect->width - 1), MAX (0, rect->height - 1));
      cairo_stroke (cr);
    }
}

static gboolean
vnck_pager_draw (CtkWidget *widget,
                 cairo_t   *cr)
{
  VnckPager *pager;
  int i;
  int n_spaces;
  VnckWorkspace *active_space;
  GdkPixbuf *bg_pixbuf;
  gboolean first;
  CtkStyleContext *context;
  CtkStateFlags state;

  pager = VNCK_PAGER (widget);

  n_spaces = vnck_screen_get_workspace_count (pager->priv->screen);
  active_space = vnck_screen_get_active_workspace (pager->priv->screen);
  bg_pixbuf = NULL;
  first = TRUE;

  state = ctk_widget_get_state_flags (widget);
  context = ctk_widget_get_style_context (widget);

  ctk_render_background (context, cr, 0, 0,
                         ctk_widget_get_allocated_width (widget),
                         ctk_widget_get_allocated_height (widget));

  ctk_style_context_save (context);
  ctk_style_context_set_state (context, state);

  if (ctk_widget_has_focus (widget))
  {
    cairo_save (cr);
    ctk_render_focus (context, cr,
		      0, 0,
		      ctk_widget_get_allocated_width (widget),
		      ctk_widget_get_allocated_height (widget));
    cairo_restore (cr);
  }

  if (pager->priv->shadow_type != CTK_SHADOW_NONE)
    {
      cairo_save (cr);
      ctk_render_frame (context, cr, 0, 0,
                        ctk_widget_get_allocated_width (widget),
                        ctk_widget_get_allocated_height (widget));
      cairo_restore (cr);
    }

  ctk_style_context_restore (context);

  i = 0;
  while (i < n_spaces)
    {
      CdkRectangle rect;

      if (pager->priv->show_all_workspaces ||
	  (active_space && i == vnck_workspace_get_number (active_space)))
	{
	  get_workspace_rect (pager, i, &rect);

          /* We only want to do this once, even if w/h change,
           * for efficiency. width/height will only change by
           * one pixel at most.
           */
          if (first &&
              pager->priv->display_mode == VNCK_PAGER_DISPLAY_CONTENT)
            {
              bg_pixbuf = vnck_pager_get_background (pager,
                                                     rect.width,
                                                     rect.height);
              first = FALSE;
            }

	  vnck_pager_draw_workspace (pager, cr, i, &rect, bg_pixbuf);
	}

      ++i;
    }

  return FALSE;
}

static gboolean
vnck_pager_button_press (CtkWidget      *widget,
                         CdkEventButton *event)
{
  VnckPager *pager;
  int space_number;
  VnckWorkspace *space = NULL;
  CdkRectangle workspace_rect;

  if (event->button != 1)
    return FALSE;

  pager = VNCK_PAGER (widget);

  space_number = workspace_at_point (pager, event->x, event->y, NULL, NULL);

  if (space_number != -1)
  {
    get_workspace_rect (pager, space_number, &workspace_rect);
    space = vnck_screen_get_workspace (pager->priv->screen, space_number);
  }

  if (space)
    {
      /* always save the start coordinates so we can know if we need to change
       * workspace when the button is released (ie, start and end coordinates
       * should be in the same workspace) */
      pager->priv->drag_start_x = event->x;
      pager->priv->drag_start_y = event->y;
    }

  if (space && (pager->priv->display_mode != VNCK_PAGER_DISPLAY_NAME))
    {
      pager->priv->drag_window = window_at_point (pager, space,
                                                  &workspace_rect,
                                                  event->x, event->y);
    }

  return TRUE;
}

static gboolean
vnck_pager_drag_motion_timeout (gpointer data)
{
  VnckPager *pager = VNCK_PAGER (data);
  VnckWorkspace *active_workspace, *dnd_workspace;

  pager->priv->dnd_activate = 0;
  active_workspace = vnck_screen_get_active_workspace (pager->priv->screen);
  dnd_workspace    = vnck_screen_get_workspace (pager->priv->screen,
                                                pager->priv->prelight);

  if (dnd_workspace &&
      (pager->priv->prelight != vnck_workspace_get_number (active_workspace)))
    vnck_workspace_activate (dnd_workspace, pager->priv->dnd_time);

  return FALSE;
}

static void
vnck_pager_queue_draw_workspace (VnckPager *pager,
                                 gint       i)
{
  CdkRectangle rect;

  if (i < 0)
    return;

  get_workspace_rect (pager, i, &rect);
  ctk_widget_queue_draw_area (CTK_WIDGET (pager),
                              rect.x, rect.y,
			      rect.width, rect.height);
}

static void
vnck_pager_queue_draw_window (VnckPager  *pager,
                              VnckWindow *window)
{
  gint workspace;

  workspace = vnck_pager_window_get_workspace (window, TRUE);
  if (workspace == -1)
    return;

  vnck_pager_queue_draw_workspace (pager, workspace);
}

static void
vnck_pager_check_prelight (VnckPager *pager,
                           gint       x,
                           gint       y,
                           gboolean   prelight_dnd)
{
  gint id;

  if (x < 0 || y < 0)
    id = -1;
  else
    id = workspace_at_point (pager, x, y, NULL, NULL);

  if (id != pager->priv->prelight)
    {
      vnck_pager_queue_draw_workspace (pager, pager->priv->prelight);
      vnck_pager_queue_draw_workspace (pager, id);
      pager->priv->prelight = id;
      pager->priv->prelight_dnd = prelight_dnd;
    }
  else if (prelight_dnd != pager->priv->prelight_dnd)
    {
      vnck_pager_queue_draw_workspace (pager, pager->priv->prelight);
      pager->priv->prelight_dnd = prelight_dnd;
    }
}

static gboolean
vnck_pager_drag_motion (CtkWidget          *widget,
                        CdkDragContext     *context,
                        gint                x,
                        gint                y,
                        guint               time)
{
  VnckPager *pager;
  gint previous_workspace;

  pager = VNCK_PAGER (widget);

  previous_workspace = pager->priv->prelight;
  vnck_pager_check_prelight (pager, x, y, TRUE);

  if (ctk_drag_dest_find_target (widget, context, NULL))
    {
      cdk_drag_status (context, cdk_drag_context_get_suggested_action (context), time);
    }
  else
    {
      cdk_drag_status (context, 0, time);

      if (pager->priv->prelight != previous_workspace &&
          pager->priv->dnd_activate != 0)
        {
          /* remove timeout, the window we hover over changed */
          g_source_remove (pager->priv->dnd_activate);
          pager->priv->dnd_activate = 0;
          pager->priv->dnd_time = 0;
        }

      if (pager->priv->dnd_activate == 0 && pager->priv->prelight > -1)
        {
          pager->priv->dnd_activate = g_timeout_add_seconds (VNCK_ACTIVATE_TIMEOUT,
                                                             vnck_pager_drag_motion_timeout,
                                                             pager);
          pager->priv->dnd_time = time;
        }
    }

  return (pager->priv->prelight != -1);
}

static gboolean
vnck_pager_drag_drop  (CtkWidget        *widget,
		       CdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
  VnckPager *pager = VNCK_PAGER (widget);
  CdkAtom target;

  target = ctk_drag_dest_find_target (widget, context, NULL);

  if (target != CDK_NONE)
    ctk_drag_get_data (widget, context, target, time);
  else
    ctk_drag_finish (context, FALSE, FALSE, time);

  vnck_pager_clear_drag (pager);
  vnck_pager_check_prelight (pager, x, y, FALSE);

  return TRUE;
}

static void
vnck_pager_drag_data_received (CtkWidget          *widget,
	  	               CdkDragContext     *context,
			       gint                x,
			       gint                y,
			       CtkSelectionData   *selection_data,
			       guint               info,
			       guint               time)
{
  VnckPager *pager = VNCK_PAGER (widget);
  VnckWorkspace *space;
  GList *tmp;
  gint i;
  gulong xid;

  if ((ctk_selection_data_get_length (selection_data) != sizeof (gulong)) ||
      (ctk_selection_data_get_format (selection_data) != 8))
    {
      ctk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  i = workspace_at_point (pager, x, y, NULL, NULL);
  space = vnck_screen_get_workspace (pager->priv->screen, i);
  if (!space)
    {
      ctk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  xid = *((gulong *) ctk_selection_data_get_data (selection_data));

  for (tmp = vnck_screen_get_windows_stacked (pager->priv->screen); tmp != NULL; tmp = tmp->next)
    {
      if (vnck_window_get_xid (tmp->data) == xid)
	{
	  VnckWindow *win = tmp->data;
	  vnck_window_move_to_workspace (win, space);
	  if (space == vnck_screen_get_active_workspace (pager->priv->screen))
	    vnck_window_activate (win, time);
	  ctk_drag_finish (context, TRUE, FALSE, time);
	  return;
	}
    }

  ctk_drag_finish (context, FALSE, FALSE, time);
}

static void
vnck_pager_drag_data_get (CtkWidget        *widget,
                          CdkDragContext   *context,
                          CtkSelectionData *selection_data,
                          guint             info,
                          guint             time)
{
  VnckPager *pager = VNCK_PAGER (widget);
  gulong xid;

  if (pager->priv->drag_window == NULL)
    return;

  xid = vnck_window_get_xid (pager->priv->drag_window);
  ctk_selection_data_set (selection_data,
			  ctk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static void
vnck_pager_drag_end (CtkWidget        *widget,
                     CdkDragContext   *context)
{
  VnckPager *pager = VNCK_PAGER (widget);

  vnck_pager_clear_drag (pager);
}

static void
vnck_pager_drag_motion_leave (CtkWidget          *widget,
                              CdkDragContext     *context,
                              guint               time)
{
  VnckPager *pager = VNCK_PAGER (widget);

  if (pager->priv->dnd_activate != 0)
    {
      g_source_remove (pager->priv->dnd_activate);
      pager->priv->dnd_activate = 0;
    }
  pager->priv->dnd_time = 0;
  vnck_pager_check_prelight (pager, -1, -1, FALSE);
}

static void
vnck_drag_clean_up (VnckWindow     *window,
		    CdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy);

static void
vnck_drag_context_destroyed (gpointer  windowp,
                             GObject  *context)
{
  vnck_drag_clean_up (windowp, (CdkDragContext *) context, TRUE, FALSE);
}

static void
vnck_update_drag_icon (VnckWindow     *window,
		       CdkDragContext *context)
{
  gint org_w, org_h, dnd_w, dnd_h;
  VnckWorkspace *workspace;
  CdkRectangle rect;
  cairo_surface_t *surface;
  CtkWidget *widget;
  cairo_t *cr;

  widget = g_object_get_data (G_OBJECT (context), "vnck-drag-source-widget");
  if (!widget)
    return;

  if (!ctk_icon_size_lookup (CTK_ICON_SIZE_DND, &dnd_w, &dnd_h))
    dnd_w = dnd_h = 32;
  /* windows are huge, so let's make this huge */
  dnd_w *= 3;

  workspace = vnck_window_get_workspace (window);
  if (workspace == NULL)
    workspace = vnck_screen_get_active_workspace (vnck_window_get_screen (window));
  if (workspace == NULL)
    return;

  vnck_window_get_geometry (window, NULL, NULL, &org_w, &org_h);

  rect.x = rect.y = 0;
  rect.width = 0.5 + ((double) (dnd_w * org_w) / (double) vnck_workspace_get_width (workspace));
  rect.width = MIN (org_w, rect.width);
  rect.height = 0.5 + ((double) (rect.width * org_h) / (double) org_w);

  /* we need at least three pixels to draw the smallest window */
  rect.width = MAX (rect.width, 3);
  rect.height = MAX (rect.height, 3);

  surface = cdk_window_create_similar_surface (ctk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               rect.width, rect.height);
  cr = cairo_create (surface);
  draw_window (cr, widget, window, &rect, CTK_STATE_FLAG_NORMAL, FALSE);
  cairo_destroy (cr);
  cairo_surface_set_device_offset (surface, 2, 2);

  ctk_drag_set_icon_surface (context, surface);

  cairo_surface_destroy (surface);
}

static void
vnck_drag_window_destroyed (gpointer  contextp,
                            GObject  *window)
{
  vnck_drag_clean_up ((VnckWindow *) window, CDK_DRAG_CONTEXT (contextp),
                      FALSE, TRUE);
}

static void
vnck_drag_source_destroyed (gpointer  contextp,
                            GObject  *drag_source)
{
  g_object_steal_data (G_OBJECT (contextp), "vnck-drag-source-widget");
}

/* CAREFUL: This function is a bit brittle, because the pointers given may be
 * finalized already */
static void
vnck_drag_clean_up (VnckWindow     *window,
		    CdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy)
{
  if (clean_up_for_context_destroy)
    {
      CtkWidget *drag_source;

      drag_source = g_object_get_data (G_OBJECT (context),
                                       "vnck-drag-source-widget");
      if (drag_source)
        g_object_weak_unref (G_OBJECT (drag_source),
                             vnck_drag_source_destroyed, context);

      g_object_weak_unref (G_OBJECT (window),
                           vnck_drag_window_destroyed, context);
      if (g_signal_handlers_disconnect_by_func (window,
                                                vnck_update_drag_icon, context) != 2)
	g_assert_not_reached ();
    }

  if (clean_up_for_window_destroy)
    {
      g_object_steal_data (G_OBJECT (context), "vnck-drag-source-widget");
      g_object_weak_unref (G_OBJECT (context),
                           vnck_drag_context_destroyed, window);
    }
}

/**
 * vnck_window_set_as_drag_icon:
 * @window: #VnckWindow to use as drag icon
 * @context: #CdkDragContext to set the icon on
 * @drag_source: #CtkWidget that started the drag, or one of its parent. This
 * widget needs to stay alive as long as possible during the drag.
 *
 * Sets the given @window as the drag icon for @context.
 **/
void
_vnck_window_set_as_drag_icon (VnckWindow     *window,
                               CdkDragContext *context,
                               CtkWidget      *drag_source)
{
  g_return_if_fail (VNCK_IS_WINDOW (window));
  g_return_if_fail (CDK_IS_DRAG_CONTEXT (context));

  g_object_weak_ref (G_OBJECT (window), vnck_drag_window_destroyed, context);
  g_signal_connect (window, "geometry_changed",
                    G_CALLBACK (vnck_update_drag_icon), context);
  g_signal_connect (window, "icon_changed",
                    G_CALLBACK (vnck_update_drag_icon), context);

  g_object_set_data (G_OBJECT (context), "vnck-drag-source-widget", drag_source);
  g_object_weak_ref (G_OBJECT (drag_source), vnck_drag_source_destroyed, context);

  g_object_weak_ref (G_OBJECT (context), vnck_drag_context_destroyed, window);

  vnck_update_drag_icon (window, context);
}

static gboolean
vnck_pager_motion (CtkWidget        *widget,
                   CdkEventMotion   *event)
{
  VnckPager *pager;
  CdkWindow *window;
  CdkSeat *seat;
  CdkDevice *pointer;
  int x, y;

  pager = VNCK_PAGER (widget);

  seat = cdk_display_get_default_seat (ctk_widget_get_display (widget));
  window = ctk_widget_get_window (widget);
  pointer = cdk_seat_get_pointer (seat);
  cdk_window_get_device_position (window, pointer, &x, &y, NULL);

  if (!pager->priv->dragging &&
      pager->priv->drag_window != NULL &&
      ctk_drag_check_threshold (widget,
                                pager->priv->drag_start_x,
                                pager->priv->drag_start_y,
                                x, y))
    {
      CtkTargetList *target_list;
      CdkDragContext *context;

      target_list = ctk_drag_dest_get_target_list (widget);
      context = ctk_drag_begin_with_coordinates (widget, target_list,
                                                 CDK_ACTION_MOVE,
                                                 1, (CdkEvent *) event,
                                                 -1, -1);

      pager->priv->dragging = TRUE;
      pager->priv->prelight_dnd = TRUE;
      _vnck_window_set_as_drag_icon (pager->priv->drag_window,
				     context,
				     CTK_WIDGET (pager));
    }

  vnck_pager_check_prelight (pager, x, y, pager->priv->prelight_dnd);

  return TRUE;
}

static gboolean
vnck_pager_leave_notify	 (CtkWidget          *widget,
	      		  CdkEventCrossing   *event)
{
  VnckPager *pager;

  pager = VNCK_PAGER (widget);

  vnck_pager_check_prelight (pager, -1, -1, FALSE);

  return FALSE;
}

static gboolean
vnck_pager_button_release (CtkWidget        *widget,
                           CdkEventButton   *event)
{
  VnckWorkspace *space;
  VnckPager *pager;
  int i;
  int j;
  int viewport_x;
  int viewport_y;

  if (event->button != 1)
    return FALSE;

  pager = VNCK_PAGER (widget);

  if (!pager->priv->dragging)
    {
      i = workspace_at_point (pager,
                              event->x, event->y,
                              &viewport_x, &viewport_y);
      j = workspace_at_point (pager,
                              pager->priv->drag_start_x,
                              pager->priv->drag_start_y,
                              NULL, NULL);

      if (i == j && i >= 0 &&
          (space = vnck_screen_get_workspace (pager->priv->screen, i)))
        {
          int screen_width, screen_height;

          /* Don't switch the desktop if we're already there */
          if (space != vnck_screen_get_active_workspace (pager->priv->screen))
            vnck_workspace_activate (space, event->time);

          /* EWMH only lets us move the viewport for the active workspace,
           * but we just go ahead and hackily assume that the activate
           * just above takes effect prior to moving the viewport
           */

          /* Transform the pointer location to viewport origin, assuming
           * that we want the nearest "regular" viewport containing the
           * pointer.
           */
          screen_width  = vnck_screen_get_width  (pager->priv->screen);
          screen_height = vnck_screen_get_height (pager->priv->screen);
          viewport_x = (viewport_x / screen_width)  * screen_width;
          viewport_y = (viewport_y / screen_height) * screen_height;

          if (vnck_workspace_get_viewport_x (space) != viewport_x ||
              vnck_workspace_get_viewport_y (space) != viewport_y)
            vnck_screen_move_viewport (pager->priv->screen, viewport_x, viewport_y);
        }

      vnck_pager_clear_drag (pager);
    }

  return FALSE;
}

static gboolean
vnck_pager_scroll_event (CtkWidget      *widget,
                         CdkEventScroll *event)
{
  VnckPager          *pager;
  VnckWorkspace      *space;
  CdkScrollDirection  absolute_direction;
  int                 index;
  int                 n_workspaces;
  int                 n_columns;
  int                 in_last_row;
  gboolean            wrap_workspaces;
  gdouble             smooth_x;
  gdouble             smooth_y;

  pager = VNCK_PAGER (widget);

  if (event->type != CDK_SCROLL)
    return FALSE;
  if (event->direction == CDK_SCROLL_SMOOTH)
    return FALSE;

  absolute_direction = event->direction;

  space = vnck_screen_get_active_workspace (pager->priv->screen);
  index = vnck_workspace_get_number (space);

  n_workspaces = vnck_screen_get_workspace_count (pager->priv->screen);
  n_columns = n_workspaces / pager->priv->n_rows;
  if (n_workspaces % pager->priv->n_rows != 0)
    n_columns++;
  in_last_row = n_workspaces % n_columns;
  wrap_workspaces = pager->priv->wrap_on_scroll;

  if (ctk_widget_get_direction (CTK_WIDGET (pager)) == CTK_TEXT_DIR_RTL)
    {
      switch (event->direction)
        {
          case CDK_SCROLL_DOWN:
          case CDK_SCROLL_UP:
            break;
          case CDK_SCROLL_RIGHT:
            absolute_direction = CDK_SCROLL_LEFT;
            break;
          case CDK_SCROLL_LEFT:
            absolute_direction = CDK_SCROLL_RIGHT;
            break;
          case CDK_SCROLL_SMOOTH:
            cdk_event_get_scroll_deltas ((CdkEvent*)event, &smooth_x, &smooth_y);
            if (smooth_x > 5)
              absolute_direction = CDK_SCROLL_RIGHT;
            else if (smooth_x < -5)
              absolute_direction = CDK_SCROLL_LEFT;
            break;
          default:
            break;
        }
    }

  if (pager->priv->scroll_mode == VNCK_PAGER_SCROLL_2D)
    {
      switch (absolute_direction)
        {
          case CDK_SCROLL_DOWN:
            if (index + n_columns < n_workspaces)
              {
                index += n_columns;
              }
            else if (wrap_workspaces && index == n_workspaces - 1)
              {
                index = 0;
              }
            else if ((index < n_workspaces - 1 &&
                      index + in_last_row != n_workspaces - 1) ||
                     (index == n_workspaces - 1 &&
                      in_last_row != 0))
              {
                index = (index % n_columns) + 1;
              }
            break;
          case CDK_SCROLL_RIGHT:
            if (index < n_workspaces - 1)
              {
                index++;
              }
            else if (wrap_workspaces)
              {
                index = 0;
              }
            break;
          case CDK_SCROLL_UP:
            if (index - n_columns >= 0)
              {
                index -= n_columns;
              }
            else if (index > 0)
              {
                index = ((pager->priv->n_rows - 1) * n_columns) + (index % n_columns) - 1;
              }
            else if (wrap_workspaces)
              {
                index = n_workspaces - 1;
              }
            if (index >= n_workspaces)
              {
                index -= n_columns;
              }
            break;
          case CDK_SCROLL_LEFT:
            if (index > 0)
              {
                index--;
              }
            else if (wrap_workspaces)
              {
                index = n_workspaces - 1;
              }
            break;
          case CDK_SCROLL_SMOOTH:
          default:
            g_assert_not_reached ();
            break;
        }
      }
    else
      {
        switch (absolute_direction)
          {
            case CDK_SCROLL_UP:
            case CDK_SCROLL_LEFT:
              if (index > 0)
                {
                  index--;
                }
              else if (wrap_workspaces)
                {
                  index = n_workspaces - 1;
                }
              break;
            case CDK_SCROLL_DOWN:
            case CDK_SCROLL_RIGHT:
              if (index < n_workspaces - 1)
                {
                  index++;
                }
              else if (wrap_workspaces)
                {
                  index = 0;
                }
              break;
            case CDK_SCROLL_SMOOTH:
            default:
              g_assert_not_reached ();
              break;
          }
      }

  space = vnck_screen_get_workspace (pager->priv->screen, index);
  vnck_workspace_activate (space, event->time);

  return TRUE;
}

static gboolean
vnck_pager_query_tooltip (CtkWidget  *widget,
                          gint        x,
                          gint        y,
                          gboolean    keyboard_tip,
                          CtkTooltip *tooltip)
{
  int i;
  VnckPager *pager;
  VnckScreen *screen;
  VnckWorkspace *space;
  char *name;

  pager = VNCK_PAGER (widget);
  screen = pager->priv->screen;

  i = workspace_at_point (pager, x, y, NULL, NULL);
  space = vnck_screen_get_workspace (screen, i);
  if (!space)
    return CTK_WIDGET_CLASS (vnck_pager_parent_class)->query_tooltip (widget,
                                                                      x, y,
                                                                      keyboard_tip,
                                                                      tooltip);

  if (vnck_screen_get_active_workspace (screen) == space)
    {
      VnckWindow *window;
      CdkRectangle workspace_rect;

      get_workspace_rect (pager, i, &workspace_rect);

      window = window_at_point (pager, space, &workspace_rect, x, y);

      if (window)
        name = g_strdup_printf (_("Click to start dragging \"%s\""),
                                vnck_window_get_name (window));
      else
        name = g_strdup_printf (_("Current workspace: \"%s\""),
                                vnck_workspace_get_name (space));
    }
  else
    {
      name = g_strdup_printf (_("Click to switch to \"%s\""),
                              vnck_workspace_get_name (space));
    }

  ctk_tooltip_set_text (tooltip, name);

  g_free (name);

  return TRUE;
}

/**
 * vnck_pager_new:
 *
 * Creates a new #VnckPager. The #VnckPager will show the #VnckWorkspace of the
 * #VnckScreen it is on.
 *
 * Return value: a newly created #VnckPager.
 */
CtkWidget*
vnck_pager_new (void)
{
  VnckPager *pager;

  pager = g_object_new (VNCK_TYPE_PAGER, NULL);

  return CTK_WIDGET (pager);
}

static gboolean
vnck_pager_set_layout_hint (VnckPager *pager)
{
  int layout_rows;
  int layout_cols;

  /* if we're not realized, we don't know about our screen yet */
  if (pager->priv->screen == NULL)
    _vnck_pager_set_screen (pager);
  /* can still happen if the pager was not added to a widget hierarchy */
  if (pager->priv->screen == NULL)
    return FALSE;

  /* The visual representation of the pager doesn't
   * correspond to the layout of the workspaces
   * here. i.e. the user will not pay any attention
   * to the n_rows setting on this pager.
   */
  if (!pager->priv->show_all_workspaces)
    return FALSE;

  if (pager->priv->orientation == CTK_ORIENTATION_HORIZONTAL)
    {
      layout_rows = pager->priv->n_rows;
      layout_cols = 0;
    }
  else
    {
      layout_rows = 0;
      layout_cols = pager->priv->n_rows;
    }

  pager->priv->layout_manager_token =
    vnck_screen_try_set_workspace_layout (pager->priv->screen,
                                          pager->priv->layout_manager_token,
                                          layout_rows,
                                          layout_cols);

  return (pager->priv->layout_manager_token != VNCK_NO_MANAGER_TOKEN);
}

/**
 * vnck_pager_set_orientation:
 * @pager: a #VnckPager.
 * @orientation: orientation to use for the layout of #VnckWorkspace on the
 * #VnckScreen @pager is watching.
 *
 * Tries to change the orientation of the layout of #VnckWorkspace on the
 * #VnckScreen @pager is watching. Since no more than one application should
 * set this property of a #VnckScreen at a time, setting the layout is not
 * guaranteed to work.
 *
 * If @orientation is %CTK_ORIENTATION_HORIZONTAL, the #VnckWorkspace will be
 * laid out in rows, with the first #VnckWorkspace in the top left corner.
 *
 * If @orientation is %CTK_ORIENTATION_VERTICAL, the #VnckWorkspace will be
 * laid out in columns, with the first #VnckWorkspace in the top left corner.
 *
 * For example, if the layout contains one row, but the orientation of the
 * layout is vertical, the #VnckPager will display a column of #VnckWorkspace.
 *
 * Note that setting the orientation will have an effect on the geometry
 * management: if @orientation is %CTK_ORIENTATION_HORIZONTAL,
 * %CTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT will be used as request mode; if
 * @orientation is %CTK_ORIENTATION_VERTICAL, CTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH
 * will be used instead.
 *
 * If @pager has not been added to a widget hierarchy, the call will fail
 * because @pager can't know the screen on which to modify the orientation.
 *
 * Return value: %TRUE if the layout of #VnckWorkspace has been successfully
 * changed or did not need to be changed, %FALSE otherwise.
 */
gboolean
vnck_pager_set_orientation (VnckPager     *pager,
                            CtkOrientation orientation)
{
  CtkOrientation old_orientation;
  gboolean       old_orientation_is_valid;

  g_return_val_if_fail (VNCK_IS_PAGER (pager), FALSE);

  if (pager->priv->orientation == orientation)
    return TRUE;

  old_orientation = pager->priv->orientation;
  old_orientation_is_valid = pager->priv->screen != NULL;

  pager->priv->orientation = orientation;

  if (vnck_pager_set_layout_hint (pager))
    {
      ctk_widget_queue_resize (CTK_WIDGET (pager));
      return TRUE;
    }
  else
    {
      if (old_orientation_is_valid)
        pager->priv->orientation = old_orientation;
      return FALSE;
    }
}

/**
 * vnck_pager_set_n_rows:
 * @pager: a #VnckPager.
 * @n_rows: the number of rows to use for the layout of #VnckWorkspace on the
 * #VnckScreen @pager is watching.
 *
 * Tries to change the number of rows in the layout of #VnckWorkspace on the
 * #VnckScreen @pager is watching. Since no more than one application should
 * set this property of a #VnckScreen at a time, setting the layout is not
 * guaranteed to work.
 *
 * If @pager has not been added to a widget hierarchy, the call will fail
 * because @pager can't know the screen on which to modify the layout.
 *
 * Return value: %TRUE if the layout of #VnckWorkspace has been successfully
 * changed or did not need to be changed, %FALSE otherwise.
 */
gboolean
vnck_pager_set_n_rows (VnckPager *pager,
		       int        n_rows)
{
  int      old_n_rows;
  gboolean old_n_rows_is_valid;

  g_return_val_if_fail (VNCK_IS_PAGER (pager), FALSE);
  g_return_val_if_fail (n_rows > 0, FALSE);

  if (pager->priv->n_rows == n_rows)
    return TRUE;

  old_n_rows = pager->priv->n_rows;
  old_n_rows_is_valid = pager->priv->screen != NULL;

  pager->priv->n_rows = n_rows;

  if (vnck_pager_set_layout_hint (pager))
    {
      ctk_widget_queue_resize (CTK_WIDGET (pager));
      return TRUE;
    }
  else
    {
      if (old_n_rows_is_valid)
        pager->priv->n_rows = old_n_rows;
      return FALSE;
    }
}

/**
 * vnck_pager_set_display_mode:
 * @pager: a #VnckPager.
 * @mode: a display mode.
 *
 * Sets the display mode for @pager to @mode.
 */
void
vnck_pager_set_display_mode (VnckPager            *pager,
			     VnckPagerDisplayMode  mode)
{
  g_return_if_fail (VNCK_IS_PAGER (pager));

  if (pager->priv->display_mode == mode)
    return;

  g_object_set (pager, "has-tooltip", mode != VNCK_PAGER_DISPLAY_NAME, NULL);

  pager->priv->display_mode = mode;
  ctk_widget_queue_resize (CTK_WIDGET (pager));
}

/**
 * vnck_pager_set_scroll_mode:
 * @pager: a #VnckPager.
 * @scroll_mode: a scroll mode.
 *
 * Sets @pager to react to input device scrolling in one of the
 * available scroll modes.
 */
void
vnck_pager_set_scroll_mode (VnckPager           *pager,
                            VnckPagerScrollMode  scroll_mode)
{
  g_return_if_fail (VNCK_IS_PAGER (pager));

  if (pager->priv->scroll_mode == scroll_mode)
    return;

  pager->priv->scroll_mode = scroll_mode;
}

/**
 * vnck_pager_set_show_all:
 * @pager: a #VnckPager.
 * @show_all_workspaces: whether to display all #VnckWorkspace in @pager.
 *
 * Sets @pager to display all #VnckWorkspace or not, according to
 * @show_all_workspaces.
 */
void
vnck_pager_set_show_all (VnckPager *pager,
			 gboolean   show_all_workspaces)
{
  g_return_if_fail (VNCK_IS_PAGER (pager));

  show_all_workspaces = (show_all_workspaces != 0);

  if (pager->priv->show_all_workspaces == show_all_workspaces)
    return;

  pager->priv->show_all_workspaces = show_all_workspaces;
  ctk_widget_queue_resize (CTK_WIDGET (pager));
}

/**
 * vnck_pager_set_shadow_type:
 * @pager: a #VnckPager.
 * @shadow_type: a shadow type.
 *
 * Sets the shadow type for @pager to @shadow_type. The main use of this
 * function is proper integration of #VnckPager in panels with non-system
 * backgrounds.
 *
 * Since: 2.2
 */
void
vnck_pager_set_shadow_type (VnckPager *   pager,
			    CtkShadowType shadow_type)
{
  g_return_if_fail (VNCK_IS_PAGER (pager));

  if (pager->priv->shadow_type == shadow_type)
    return;

  pager->priv->shadow_type = shadow_type;
  ctk_widget_queue_resize (CTK_WIDGET (pager));
}

/**
 * vnck_pager_set_wrap_on_scroll:
 * @pager: a #VnckPager.
 * @wrap_on_scroll: a boolean.
 *
 * Sets the wrapping behavior of the @pager. Setting it to %TRUE will
 * wrap arround to the start when scrolling over the end and vice
 * versa. By default it is set to %FALSE.
 *
 * Since: 3.24.0
 */
void
vnck_pager_set_wrap_on_scroll (VnckPager *pager,
                               gboolean   wrap_on_scroll)
{
  g_return_if_fail (VNCK_IS_PAGER (pager));

  pager->priv->wrap_on_scroll = wrap_on_scroll;
}

/**
 * vnck_pager_get_wrap_on_scroll:
 * @pager: a #VnckPager.
 *
 * Return value: %TRUE if the @pager wraps workspaces on a scroll event that
 * hits a border, %FALSE otherwise.
 *
 * Since: 3.24.0
 */
gboolean
vnck_pager_get_wrap_on_scroll (VnckPager *pager)
{
  g_return_val_if_fail (VNCK_IS_PAGER (pager), FALSE);

  return pager->priv->wrap_on_scroll;
}

static void
active_window_changed_callback    (VnckScreen      *screen,
                                   VnckWindow      *previous_window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  ctk_widget_queue_draw (CTK_WIDGET (pager));
}

static void
active_workspace_changed_callback (VnckScreen      *screen,
                                   VnckWorkspace   *previous_workspace,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  ctk_widget_queue_draw (CTK_WIDGET (pager));
}

static void
window_stacking_changed_callback  (VnckScreen      *screen,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  ctk_widget_queue_draw (CTK_WIDGET (pager));
}

static void
window_opened_callback            (VnckScreen      *screen,
                                   VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);

  vnck_pager_connect_window (pager, window);
  vnck_pager_queue_draw_window (pager, window);
}

static void
window_closed_callback            (VnckScreen      *screen,
                                   VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);

  if (pager->priv->drag_window == window)
    vnck_pager_clear_drag (pager);

  vnck_pager_queue_draw_window (pager, window);
}

static void
workspace_created_callback        (VnckScreen      *screen,
                                   VnckWorkspace   *space,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  g_signal_connect (space, "name_changed",
                    G_CALLBACK (workspace_name_changed_callback), pager);
  ctk_widget_queue_resize (CTK_WIDGET (pager));
}

static void
workspace_destroyed_callback      (VnckScreen      *screen,
                                   VnckWorkspace   *space,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  g_signal_handlers_disconnect_by_func (space, G_CALLBACK (workspace_name_changed_callback), pager);
  ctk_widget_queue_resize (CTK_WIDGET (pager));
}

static void
application_opened_callback       (VnckScreen      *screen,
                                   VnckApplication *app,
                                   gpointer         data)
{
  /*   VnckPager *pager = VNCK_PAGER (data); */
}

static void
application_closed_callback       (VnckScreen      *screen,
                                   VnckApplication *app,
                                   gpointer         data)
{
  /*   VnckPager *pager = VNCK_PAGER (data); */
}

static void
window_name_changed_callback      (VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  vnck_pager_queue_draw_window (pager, window);
}

static void
window_state_changed_callback     (VnckWindow      *window,
                                   VnckWindowState  changed,
                                   VnckWindowState  new,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);

  /* if the changed state changes the visibility in the pager, we need to
   * redraw the whole workspace. vnck_pager_queue_draw_window() might not be
   * enough */
  if (!vnck_pager_window_state_is_relevant (changed))
    vnck_pager_queue_draw_workspace (pager,
                                     vnck_pager_window_get_workspace (window,
                                                                      FALSE));
  else
    vnck_pager_queue_draw_window (pager, window);
}

static void
window_workspace_changed_callback (VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  ctk_widget_queue_draw (CTK_WIDGET (pager));
}

static void
window_icon_changed_callback      (VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);
  vnck_pager_queue_draw_window (pager, window);
}

static void
window_geometry_changed_callback  (VnckWindow      *window,
                                   gpointer         data)
{
  VnckPager *pager = VNCK_PAGER (data);

  vnck_pager_queue_draw_window (pager, window);
}

static void
background_changed_callback (VnckWindow *window,
                             gpointer    data)
{
  VnckPager *pager = VNCK_PAGER (data);

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  ctk_widget_queue_draw (CTK_WIDGET (pager));
}

static void
workspace_name_changed_callback (VnckWorkspace *space,
                                 gpointer       data)
{
  ctk_widget_queue_resize (CTK_WIDGET (data));
}

static void
viewports_changed_callback (VnckWorkspace *space,
                            gpointer       data)
{
  ctk_widget_queue_resize (CTK_WIDGET (data));
}

static void
vnck_pager_connect_screen (VnckPager *pager)
{
  int i;
  guint *c;
  GList *tmp;
  VnckScreen *screen;

  g_return_if_fail (pager->priv->screen != NULL);

  screen = pager->priv->screen;

  for (tmp = vnck_screen_get_windows (screen); tmp; tmp = tmp->next)
    {
      vnck_pager_connect_window (pager, VNCK_WINDOW (tmp->data));
    }

  i = 0;
  c = pager->priv->screen_connections;

  c[i] = g_signal_connect (G_OBJECT (screen), "active_window_changed",
                           G_CALLBACK (active_window_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "active_workspace_changed",
                           G_CALLBACK (active_workspace_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_stacking_changed",
                           G_CALLBACK (window_stacking_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_opened",
                           G_CALLBACK (window_opened_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "window_closed",
                           G_CALLBACK (window_closed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_created",
                           G_CALLBACK (workspace_created_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "workspace_destroyed",
                           G_CALLBACK (workspace_destroyed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "application_opened",
                           G_CALLBACK (application_opened_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "application_closed",
                           G_CALLBACK (application_closed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "background_changed",
                           G_CALLBACK (background_changed_callback),
                           pager);
  ++i;

  c[i] = g_signal_connect (G_OBJECT (screen), "viewports_changed",
                           G_CALLBACK (viewports_changed_callback),
                           pager);
  ++i;

  g_assert (i == N_SCREEN_CONNECTIONS);

  /* connect to name_changed on each workspace */
  for (i = 0; i < vnck_screen_get_workspace_count (pager->priv->screen); i++)
    {
      VnckWorkspace *space;
      space = vnck_screen_get_workspace (pager->priv->screen, i);
      g_signal_connect (space, "name_changed",
                        G_CALLBACK (workspace_name_changed_callback), pager);
    }
}

static void
vnck_pager_connect_window (VnckPager  *pager,
                           VnckWindow *window)
{
  g_signal_connect (G_OBJECT (window), "name_changed",
                    G_CALLBACK (window_name_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "state_changed",
                    G_CALLBACK (window_state_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "workspace_changed",
                    G_CALLBACK (window_workspace_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "icon_changed",
                    G_CALLBACK (window_icon_changed_callback),
                    pager);
  g_signal_connect (G_OBJECT (window), "geometry_changed",
                    G_CALLBACK (window_geometry_changed_callback),
                    pager);
}

static void
vnck_pager_disconnect_screen (VnckPager  *pager)
{
  int i;
  GList *tmp;

  if (pager->priv->screen == NULL)
    return;

  i = 0;
  while (i < N_SCREEN_CONNECTIONS)
    {
      if (pager->priv->screen_connections[i] != 0)
        g_signal_handler_disconnect (G_OBJECT (pager->priv->screen),
                                     pager->priv->screen_connections[i]);

      pager->priv->screen_connections[i] = 0;

      ++i;
    }

  for (i = 0; i < vnck_screen_get_workspace_count (pager->priv->screen); i++)
    {
      VnckWorkspace *space;
      space = vnck_screen_get_workspace (pager->priv->screen, i);
      g_signal_handlers_disconnect_by_func (space, G_CALLBACK (workspace_name_changed_callback), pager);
    }

  for (tmp = vnck_screen_get_windows (pager->priv->screen); tmp; tmp = tmp->next)
    {
      vnck_pager_disconnect_window (pager, VNCK_WINDOW (tmp->data));
    }
}

static void
vnck_pager_disconnect_window (VnckPager  *pager,
                              VnckWindow *window)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_name_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_state_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_workspace_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_icon_changed_callback),
                                        pager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
                                        G_CALLBACK (window_geometry_changed_callback),
                                        pager);
}

static void
vnck_pager_clear_drag (VnckPager *pager)
{
  if (pager->priv->dragging)
    vnck_pager_queue_draw_window (pager, pager->priv->drag_window);

  pager->priv->dragging = FALSE;
  pager->priv->drag_window = NULL;
  pager->priv->drag_start_x = -1;
  pager->priv->drag_start_y = -1;
}

static GdkPixbuf*
vnck_pager_get_background (VnckPager *pager,
                           int        width,
                           int        height)
{
  Pixmap p;
  GdkPixbuf *pix = NULL;

  /* We have to be careful not to keep alternating between
   * width/height values, otherwise this would get really slow.
   */
  if (pager->priv->bg_cache &&
      cdk_pixbuf_get_width (pager->priv->bg_cache) == width &&
      cdk_pixbuf_get_height (pager->priv->bg_cache) == height)
    return pager->priv->bg_cache;

  if (pager->priv->bg_cache)
    {
      g_object_unref (G_OBJECT (pager->priv->bg_cache));
      pager->priv->bg_cache = NULL;
    }

  if (pager->priv->screen == NULL)
    return NULL;

  /* FIXME this just globally disables the thumbnailing feature */
  return NULL;

#define MIN_BG_SIZE 10

  if (width < MIN_BG_SIZE || height < MIN_BG_SIZE)
    return NULL;

  p = vnck_screen_get_background_pixmap (pager->priv->screen);

  if (p != None)
    {
      Screen *xscreen;

      xscreen = VNCK_SCREEN_XSCREEN (pager->priv->screen);
      pix = _vnck_cdk_pixbuf_get_from_pixmap (xscreen, p);
    }

  if (pix)
    {
      pager->priv->bg_cache = cdk_pixbuf_scale_simple (pix,
                                                       width,
                                                       height,
                                                       CDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (pix));
    }

  return pager->priv->bg_cache;
}

/*
 *This will return aobj_pager whose parent is vnck's atk object -Gail Container
 */
static AtkObject *
vnck_pager_get_accessible (CtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type VnckPager.
       */
      derived_type = g_type_parent (VNCK_TYPE_PAGER);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry,
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);

      if (g_type_is_a (derived_atk_type, CTK_TYPE_ACCESSIBLE))
        {
          /*
           * Specify what factory to use to create accessible
           * objects
           */
          atk_registry_set_factory_type (registry,
                                         VNCK_TYPE_PAGER,
                                         VNCK_TYPE_PAGER_ACCESSIBLE_FACTORY);

          atk_registry_set_factory_type (registry,
                                         VNCK_TYPE_WORKSPACE,
                                         VNCK_TYPE_WORKSPACE_ACCESSIBLE_FACTORY);
        }
      first_time = FALSE;
    }
  return CTK_WIDGET_CLASS (vnck_pager_parent_class)->get_accessible (widget);
}

int
_vnck_pager_get_n_workspaces (VnckPager *pager)
{
  return vnck_screen_get_workspace_count (pager->priv->screen);
}

const char*
_vnck_pager_get_workspace_name (VnckPager *pager,
                                int        i)
{
  VnckWorkspace *space;

  space = vnck_screen_get_workspace (pager->priv->screen, i);
  if (space)
    return vnck_workspace_get_name (space);
  else
    return NULL;
}

VnckWorkspace*
_vnck_pager_get_active_workspace (VnckPager *pager)
{
  return vnck_screen_get_active_workspace (pager->priv->screen);
}

VnckWorkspace*
_vnck_pager_get_workspace (VnckPager *pager,
                           int        i)
{
  return vnck_screen_get_workspace (pager->priv->screen, i);
}

void
_vnck_pager_activate_workspace (VnckWorkspace *wspace,
                                guint32        timestamp)
{
  vnck_workspace_activate (wspace, timestamp);
}

void
_vnck_pager_get_workspace_rect (VnckPager    *pager,
                                int           i,
                                CdkRectangle *rect)
{
  get_workspace_rect (pager, i, rect);
}
