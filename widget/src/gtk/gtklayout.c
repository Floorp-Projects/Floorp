/* Copyright Owen Taylor, 1998
 * 
 * This file may be distributed under either the terms of the
 * Netscape Public License, or the GNU Library General Public License
 *
 * Note: No GTK+ or Mozilla code should be added to this file.
 * The coding style should be that of the the GTK core.
 */

#ifndef GTK_HAVE_FEATURES_1_1_6

#include "gtklayout.h"
#include <gtk/gtksignal.h>
#include <gdk/gdkx.h>


static void     gtk_layout_class_init         (GtkLayoutClass *class);
static void     gtk_layout_init               (GtkLayout      *layout);

static void     gtk_layout_realize            (GtkWidget      *widget);
static void     gtk_layout_unrealize          (GtkWidget      *widget);
static void     gtk_layout_map                (GtkWidget      *widget);
static void     gtk_layout_size_request       (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void     gtk_layout_size_allocate      (GtkWidget      *widget,
					       GtkAllocation  *allocation);
static void     gtk_layout_draw               (GtkWidget      *widget, 
					       GdkRectangle   *area);
static gint     gtk_layout_expose             (GtkWidget      *widget, 
					       GdkEventExpose *event);

static void     gtk_layout_remove             (GtkContainer *container, 
					       GtkWidget    *widget);
static void     gtk_layout_forall             (GtkContainer *container,
					       gboolean      include_internals,
					       GtkCallback   callback,
					       gpointer      callback_data);

static void     gtk_layout_realize_child      (GtkLayout      *layout,
					       GtkLayoutChild *child);
static void     gtk_layout_position_child     (GtkLayout      *layout,
					       GtkLayoutChild *child,
					       gboolean        force_allocate);
static void     gtk_layout_position_children  (GtkLayout      *layout);

static void     gtk_layout_expose_area        (GtkLayout      *layout,
					       gint            x, 
					       gint            y, 
					       gint            width, 
					       gint            height);
static void     gtk_layout_adjustment_changed (GtkAdjustment  *adjustment,
					       GtkLayout      *layout);
static GdkFilterReturn gtk_layout_filter      (GdkXEvent      *gdk_xevent,
					       GdkEvent       *event,
					       gpointer        data);
static GdkFilterReturn gtk_layout_main_filter (GdkXEvent      *gdk_xevent,
					       GdkEvent       *event,
					       gpointer        data);

static gboolean gtk_layout_gravity_works      (void);
static void     gtk_layout_set_static_gravity (GdkWindow *win,
					       gboolean   op);

static GtkWidgetClass *parent_class = NULL;
static gboolean gravity_works;

/* Public interface
 */
  
GtkWidget*    
gtk_layout_new (GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment)
{
  GtkLayout *layout;

  layout = gtk_type_new (gtk_layout_get_type());

  gtk_layout_set_hadjustment (layout, hadjustment);
  gtk_layout_set_vadjustment (layout, vadjustment);

  return GTK_WIDGET (layout);
}

GtkAdjustment* 
gtk_layout_get_hadjustment (GtkLayout     *layout)
{
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->hadjustment;
}
GtkAdjustment* 
gtk_layout_get_vadjustment (GtkLayout     *layout)
{
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->vadjustment;
}

void           
gtk_layout_set_hadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (layout->hadjustment)
    gtk_object_unref (GTK_OBJECT (layout->hadjustment));

  if (!adjustment)
    adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
  else
    gtk_object_ref (GTK_OBJECT (adjustment));

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (gtk_layout_adjustment_changed),
		      layout);

  layout->hadjustment = adjustment;
}
 

void           
gtk_layout_set_vadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  
  if (layout->vadjustment)
    gtk_object_unref (GTK_OBJECT (layout->hadjustment));

  if (!adjustment)
    adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
  else
    gtk_object_ref (GTK_OBJECT (adjustment));

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (gtk_layout_adjustment_changed),
		      layout);

  layout->vadjustment = adjustment;
}


void           
gtk_layout_put (GtkLayout     *layout, 
		GtkWidget     *child_widget, 
		gint           x, 
		gint           y)
{
  GtkLayoutChild *child;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  
  child = g_new (GtkLayoutChild, 1);

  child->widget = child_widget;
  child->window = NULL;
  child->x = x;
  child->y = y;
  child->widget->requisition.width = 0;
  child->widget->requisition.height = 0;

  layout->children = g_list_append (layout->children, child);
  
  gtk_widget_set_parent (child_widget, GTK_WIDGET (layout));

  gtk_widget_size_request (child->widget, &child->widget->requisition);
  
  if (GTK_WIDGET_REALIZED (layout) &&
      !GTK_WIDGET_REALIZED (child_widget))
    gtk_layout_realize_child (layout, child);

  gtk_layout_position_child (layout, child, TRUE);
}

void           
gtk_layout_move (GtkLayout     *layout, 
		 GtkWidget     *child_widget, 
		 gint           x, 
		 gint           y)
{
  GList *tmp_list;
  GtkLayoutChild *child;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == child_widget)
	{
	  child->x = x;
	  child->y = y;
	  
	  gtk_layout_position_child (layout, child, TRUE);
	  return;
	}
      tmp_list = tmp_list->next;
    }
}

void
gtk_layout_set_size (GtkLayout     *layout, 
		     guint          width,
		     guint          height)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  layout->width = width;
  layout->height = height;

  layout->hadjustment->upper = layout->width;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->hadjustment), "changed");

  layout->vadjustment->upper = layout->height;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->vadjustment), "changed");
}

void
gtk_layout_freeze (GtkLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  layout->frozen = TRUE;
}

void
gtk_layout_thaw (GtkLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (!layout->frozen)
    return;

  layout->frozen = FALSE;
  gtk_layout_position_children (layout);
  gtk_widget_draw (GTK_WIDGET (layout), NULL);
}

/* Basic Object handling procedures
 */
guint
gtk_layout_get_type (void)
{
  static guint layout_type = 0;

  if (!layout_type)
    {
      GtkTypeInfo layout_info =
      {
	"GtkLayout",
	sizeof (GtkLayout),
	sizeof (GtkLayoutClass),
	(GtkClassInitFunc) gtk_layout_class_init,
	(GtkObjectInitFunc) gtk_layout_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      layout_type = gtk_type_unique (gtk_container_get_type (), &layout_info);
    }

  return layout_type;
}

static void
gtk_layout_class_init (GtkLayoutClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  widget_class->realize = gtk_layout_realize;
  widget_class->unrealize = gtk_layout_unrealize;
  widget_class->map = gtk_layout_map;
  widget_class->size_request = gtk_layout_size_request;
  widget_class->size_allocate = gtk_layout_size_allocate;
  widget_class->draw = gtk_layout_draw;
  widget_class->expose_event = gtk_layout_expose;

  gravity_works = gtk_layout_gravity_works ();

  container_class->remove = gtk_layout_remove;
  container_class->forall = gtk_layout_forall;
}

static void
gtk_layout_init (GtkLayout *layout)
{
  layout->children = NULL;

  layout->width = 100;
  layout->height = 100;

  layout->hadjustment = NULL;
  layout->vadjustment = NULL;

  layout->bin_window = NULL;

  layout->configure_serial = 0;
  layout->scroll_x = 0;
  layout->scroll_y = 0;
  layout->visibility = GDK_VISIBILITY_PARTIAL;
}

/* Widget methods
 */

static void 
gtk_layout_realize (GtkWidget *widget)
{
  GList *tmp_list;
  GtkLayout *layout;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);
  GTK_WIDGET_SET_FLAGS (layout, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.event_mask = gtk_widget_get_events (widget);

  layout->bin_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_set_user_data (layout->bin_window, widget);

  if (gravity_works)
    gtk_layout_set_static_gravity (layout->bin_window, TRUE);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, layout->bin_window, GTK_STATE_NORMAL);

  gdk_window_add_filter (widget->window, gtk_layout_main_filter, layout);
  gdk_window_add_filter (layout->bin_window, gtk_layout_filter, layout);

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;

      if (GTK_WIDGET_VISIBLE (child->widget))
	gtk_layout_realize_child (layout, child);
	
      tmp_list = tmp_list->next;
    }
}

static void 
gtk_layout_map (GtkWidget *widget)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gdk_window_show (widget->window);
  gdk_window_show (layout->bin_window);
  
  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  !GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_map (child->widget);

      if (child->window)
	gdk_window_show (child->window);

      tmp_list = tmp_list->next;
    }
  
}

static void 
gtk_layout_unrealize (GtkWidget *widget)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  tmp_list = layout->children;

  gdk_window_set_user_data (layout->bin_window, NULL);
  gdk_window_destroy (layout->bin_window);
  layout->bin_window = NULL;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;

      if (child->window)
	{
	  gdk_window_set_user_data (child->window, NULL);
	  gdk_window_destroy (child->window);
	}
	
      tmp_list = tmp_list->next;
    }
}

static void 
gtk_layout_draw (GtkWidget *widget, GdkRectangle *area)
{
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);
}

static void     
gtk_layout_size_request (GtkWidget     *widget,
			 GtkRequisition *requisition)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      gtk_widget_size_request (child->widget, &child->widget->requisition);
      
      tmp_list = tmp_list->next;
    }
}

static void     
gtk_layout_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  widget->allocation = *allocation;
  
  layout = GTK_LAYOUT (widget);

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      gtk_layout_position_child (layout, child, TRUE);
      
      tmp_list = tmp_list->next;
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_move_resize (GTK_LAYOUT(widget)->bin_window,
			      0, 0,
			      allocation->width, allocation->height);
    }

  layout->hadjustment->page_size = allocation->width;
  layout->hadjustment->page_increment = allocation->width / 2;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->hadjustment), "changed");

  layout->vadjustment->page_size = allocation->height;
  layout->vadjustment->page_increment = allocation->height / 2;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->vadjustment), "changed");
}

static gint 
gtk_layout_expose (GtkWidget *widget, GdkEventExpose *event)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LAYOUT (widget), FALSE);

  layout = GTK_LAYOUT (widget);

  if (event->window == layout->bin_window)
    return FALSE;
  
  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;

      if (event->window == child->window)
	return gtk_widget_event (child->widget, (GdkEvent *)event);
	
      tmp_list = tmp_list->next;
    }

  return FALSE;
}

/* Container method
 */
static void
gtk_layout_remove (GtkContainer *container, 
		   GtkWidget    *widget)
{
  GList *tmp_list;
  GtkLayout *layout;
  GtkLayoutChild *child;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (container));
  
  layout = GTK_LAYOUT (container);

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      if (child->window)
	{
	  /* FIXME: This will cause problems for reparenting NO_WINDOW
	   * widgets out of a GtkLayout
	   */
	  gdk_window_set_user_data (child->window, NULL);
	  gdk_window_destroy (child->window);
	}

      gtk_widget_unparent (widget);

      layout->children = g_list_remove_link (layout->children, tmp_list);
      g_list_free_1 (tmp_list);
      g_free (child);
    }
}

static void
gtk_layout_forall (GtkContainer *container,
		   gboolean      include_internals,
		   GtkCallback   callback,
		   gpointer      callback_data)
{
  GtkLayout *layout;
  GtkLayoutChild *child;
  GList *tmp_list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (container));
  g_return_if_fail (callback != NULL);

  layout = GTK_LAYOUT (container);

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

/* Operations on children
 */

static void
gtk_layout_realize_child (GtkLayout *layout,
			  GtkLayoutChild *child)
{
  GtkWidget *widget;
  gint attributes_mask;

  widget = GTK_WIDGET (layout);
  
  if (GTK_WIDGET_NO_WINDOW (child->widget))
    {
      GdkWindowAttr attributes;
      
      gint x = child->x - layout->xoffset;
      gint y = child->y - layout->xoffset;

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = x;
      attributes.y = y;
      attributes.width = child->widget->requisition.width;
      attributes.height = child->widget->requisition.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = GDK_EXPOSURE_MASK;
 
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      child->window =  gdk_window_new (layout->bin_window,
 				       &attributes, attributes_mask);
      gdk_window_set_user_data (child->window, widget);

      if (child->window)
	gtk_style_set_background (widget->style, 
				  child->window, 
				  GTK_STATE_NORMAL);
    }

  gtk_widget_set_parent_window (child->widget,
				child->window ? child->window : layout->bin_window);
  
  gtk_widget_realize (child->widget);
  
  if (gravity_works)
    gtk_layout_set_static_gravity (child->window ? child->window : child->widget->window, TRUE);
}

static void
gtk_layout_position_child (GtkLayout      *layout,
			   GtkLayoutChild *child,
			   gboolean        force_allocate)
{
  gint x;
  gint y;

  x = child->x - layout->xoffset;
  y = child->y - layout->yoffset;
  
  if ((x >= G_MINSHORT) && (x <= G_MAXSHORT) &&
      (y >= G_MINSHORT) && (y <= G_MAXSHORT))
    {
      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  GTK_WIDGET_MAPPED (layout) &&
	  !GTK_WIDGET_MAPPED (child->widget))
	{
	  gtk_widget_map (child->widget);
	  force_allocate = TRUE;
	}
      
      if (force_allocate)
	{
	  GtkAllocation allocation;

	  if (GTK_WIDGET_NO_WINDOW (child->widget))
	    {
	      if (child->window)
		{
		  gdk_window_move_resize (child->window,
					  x, y, 
					  child->widget->requisition.width,
					  child->widget->requisition.height);
		}

	      allocation.x = 0;
	      allocation.y = 0;
	    }
	  else
	    {
	      allocation.x = x;
	      allocation.y = y;
	    }

	  allocation.width = child->widget->requisition.width;
	  allocation.height = child->widget->requisition.height;
	  
	  gtk_widget_size_allocate (child->widget, &allocation);
	}
    }
  else
    {
      if (child->window)
	gdk_window_hide (child->window);
      else if (GTK_WIDGET_MAPPED (child->widget))
 	gtk_widget_unmap (child->widget);
    }
}

static void
gtk_layout_position_children (GtkLayout *layout)
{
  GList *tmp_list;

  tmp_list = layout->children;
  while (tmp_list)
    {
      gtk_layout_position_child (layout, tmp_list->data, FALSE);

      tmp_list = tmp_list->next;
    }
}
  
/* Callbacks */

/* Send a synthetic expose event to the widget
 */
static void
gtk_layout_expose_area (GtkLayout    *layout,
			gint x, gint y, gint width, gint height)
{
  if (layout->visibility == GDK_VISIBILITY_UNOBSCURED)
    {
      GdkEventExpose event;
      
      event.type = GDK_EXPOSE;
      event.send_event = TRUE;
      event.window = layout->bin_window;
      event.count = 0;
      
      event.area.x = x;
      event.area.y = y;
      event.area.width = width;
      event.area.height = height;
      
      gdk_window_ref (event.window);
      gtk_widget_event (GTK_WIDGET (layout), (GdkEvent *)&event);
      gdk_window_unref (event.window);
    }
}

/* This function is used to find events to process while scrolling
 * Removing the GravityNotify events is a bit of a hack - currently
 * GTK uses a lot of time processing them as GtkEventOther - a
 * feature that is obsolete and will be removed. Until then...
 */

static Bool 
gtk_layout_expose_predicate (Display *display, 
		  XEvent  *xevent, 
		  XPointer arg)
{
  if ((xevent->type == Expose) || (xevent->type == GravityNotify) ||
      ((xevent->xany.window == *(Window *)arg) &&
       (xevent->type == ConfigureNotify)))
    return True;
  else
    return False;
}

/* This is the main routine to do the scrolling. Scrolling is
 * done by "Guffaw" scrolling, as in the Mozilla XFE, with
 * a few modifications.
 * 
 * The main improvement is that we keep track of whether we
 * are obscured or not. If not, we ignore the generated expose
 * events and instead do the exposes ourself, without having
 * to wait for a roundtrip to the server. This also provides
 * a limited form of expose-event compression, since we do
 * the affected area as one big chunk.
 *
 * Real expose event compression, as in the XFE, could be added
 * here. It would help opaque drags over the region, and the
 * obscured case.
 *
 * Code needs to be added here to do the scrolling on machines
 * that don't have working WindowGravity. That could be done
 *
 *  - XCopyArea and move the windows, and accept trailing the
 *    background color. (Since it is only a fallback method)
 *  - XmHTML style. As above, but turn off expose events when
 *    not obscured and do the exposures ourself.
 *  - gzilla-style. Move the window continuously, and reset
 *    every 32768 pixels
 */

static void
gtk_layout_adjustment_changed (GtkAdjustment *adjustment,
			       GtkLayout     *layout)
{
  GtkWidget *widget;
  XEvent xevent;
  
  gint dx, dy;

  widget = GTK_WIDGET (layout);

  dx = (gint)layout->hadjustment->value - layout->xoffset;
  dy = (gint)layout->vadjustment->value - layout->yoffset;

  layout->xoffset = (gint)layout->hadjustment->value;
  layout->yoffset = (gint)layout->vadjustment->value;

  if (layout->frozen)
    return;

  gtk_layout_position_children (layout);

  if (!GTK_WIDGET_MAPPED (layout))
    return;

  if (dx > 0)
    {
      if (gravity_works)
	{
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width + dx,
			     widget->allocation.height);
	  gdk_window_move   (layout->bin_window, -dx, 0);
	  gdk_window_move_resize (layout->bin_window,
				  0, 0,
				  widget->allocation.width,
				  widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout, 
			      widget->allocation.width - dx,
			      0,
			      dx,
			      widget->allocation.height);
    }
  else if (dx < 0)
    {
      if (gravity_works)
	{
	  gdk_window_move_resize (layout->bin_window,
				  dx, 0,
				  widget->allocation.width - dx,
				  widget->allocation.height);
	  gdk_window_move   (layout->bin_window, 0, 0);
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout,
			      0,
			      0,
			      -dx,
			      widget->allocation.height);
    }

  if (dy > 0)
    {
      if (gravity_works)
	{
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height + dy);
	  gdk_window_move   (layout->bin_window, 0, -dy);
	  gdk_window_move_resize (layout->bin_window,
				  0, 0,
				  widget->allocation.width,
				  widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout, 
			      0,
			      widget->allocation.height - dy,
			      widget->allocation.width,
			      dy);
    }
  else if (dy < 0)
    {
      if (gravity_works)
	{
	  gdk_window_move_resize (layout->bin_window,
				  0, dy,
				  widget->allocation.width,
				  widget->allocation.height - dy);
	  gdk_window_move   (layout->bin_window, 0, 0);
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}
      gtk_layout_expose_area (layout, 
			      0,
			      0,
			      widget->allocation.height,
			      -dy);
    }

  /* We have to make sure that all exposes from this scroll get
   * processed before we scroll again, or the expose events will
   * have invalid coordinates.
   *
   * We also do expose events for other windows, since otherwise
   * their updating will fall behind the scrolling 
   *
   * This also avoids a problem in pre-1.0 GTK where filters don't
   * have access to configure events that were compressed.
   */

  gdk_flush();
  while (XCheckIfEvent(GDK_WINDOW_XDISPLAY (layout->bin_window),
		       &xevent,
		       gtk_layout_expose_predicate,
		       (XPointer)&GDK_WINDOW_XWINDOW (layout->bin_window)))
    {
      GdkEvent event;
      GtkWidget *event_widget;

      if ((xevent.xany.window == GDK_WINDOW_XWINDOW (layout->bin_window)) &&
	  (gtk_layout_filter (&xevent, &event, layout) == GDK_FILTER_REMOVE))
	continue;
      
      if (xevent.type == Expose)
	{
	  event.expose.window = gdk_window_lookup (xevent.xany.window);
	  gdk_window_get_user_data (event.expose.window, 
				    (gpointer *)&event_widget);

	  if (event_widget)
	    {
	      event.expose.type = GDK_EXPOSE;
	      event.expose.area.x = xevent.xexpose.x;
	      event.expose.area.y = xevent.xexpose.y;
	      event.expose.area.width = xevent.xexpose.width;
	      event.expose.area.height = xevent.xexpose.height;
	      event.expose.count = xevent.xexpose.count;
	      
	      gdk_window_ref (event.expose.window);
	      gtk_widget_event (event_widget, &event);
	      gdk_window_unref (event.expose.window);
	    }
	}
    }
}

/* The main event filter. Actually, we probably don't really need
 * to install this as a filter at all, since we are calling it
 * directly above in the expose-handling hack. But in case scrollbars
 * are fixed up in some manner...
 *
 * This routine identifies expose events that are generated when
 * we've temporarily moved the bin_window_origin, and translates
 * them or discards them, depending on whether we are obscured
 * or not.
 */
static GdkFilterReturn 
gtk_layout_filter (GdkXEvent *gdk_xevent,
		   GdkEvent  *event,
		   gpointer   data)
{
  XEvent *xevent;
  GtkLayout *layout;

  xevent = (XEvent *)gdk_xevent;
  layout = GTK_LAYOUT (data);

  switch (xevent->type)
    {
    case Expose:
      if (xevent->xexpose.serial == layout->configure_serial)
	{
	  if (layout->visibility == GDK_VISIBILITY_UNOBSCURED)
	    return GDK_FILTER_REMOVE;
	  else
	    {
	      xevent->xexpose.x += layout->scroll_x;
	      xevent->xexpose.y += layout->scroll_y;
	      
	      break;
	    }
	}
      break;
      
    case ConfigureNotify:
      if ((xevent->xconfigure.x != 0) || (xevent->xconfigure.y != 0))
	{
	  layout->configure_serial = xevent->xconfigure.serial;
	  layout->scroll_x = xevent->xconfigure.x;
	  layout->scroll_y = xevent->xconfigure.y;
	}
      break;
    }
  
  return GDK_FILTER_CONTINUE;
}

/* Although GDK does have a GDK_VISIBILITY_NOTIFY event,
 * there is no corresponding event in GTK, so we have
 * to get the events from a filter
 */
static GdkFilterReturn 
gtk_layout_main_filter (GdkXEvent *gdk_xevent,
			GdkEvent  *event,
			gpointer   data)
{
  XEvent *xevent;
  GtkLayout *layout;

  xevent = (XEvent *)gdk_xevent;
  layout = GTK_LAYOUT (data);

  if (xevent->type == VisibilityNotify)
    {
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  layout->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;

	case VisibilityPartiallyObscured:
	  layout->visibility = GDK_VISIBILITY_PARTIAL;
	  break;

	case VisibilityUnobscured:
	  layout->visibility = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}

      return GDK_FILTER_REMOVE;
    }

  
  return GDK_FILTER_CONTINUE;
}

/* Routines to set the window gravity, and check whether it is
 * functional. Extra capabilities need to be added to GDK, so
 * we don't have to use Xlib here.
 */
static void
gtk_layout_set_static_gravity (GdkWindow *win, gboolean on)
{
  XSetWindowAttributes xattributes;

  xattributes.win_gravity = on ? StaticGravity : NorthWestGravity;
  xattributes.bit_gravity = on ? StaticGravity : NorthWestGravity;
  
  XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (win),
			   GDK_WINDOW_XWINDOW (win),
			   CWBitGravity | CWWinGravity,
			   &xattributes);
}

static gboolean
gtk_layout_gravity_works (void)
{
  GdkWindowAttr attr;
  
  GdkWindow *parent;
  GdkWindow *child;
  gint y;

  /* This particular server apparently has a bug so that the test
   * works but the actual code crashes it
   */
  if ((!strcmp (XServerVendor (GDK_DISPLAY()), "Sun Microsystems, Inc.")) &&
      (VendorRelease (GDK_DISPLAY()) == 3400))
    return FALSE;

  attr.window_type = GDK_WINDOW_TEMP;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = 0;
  attr.y = 0;
  attr.width = 100;
  attr.height = 100;
  attr.event_mask = 0;
  
  parent = gdk_window_new (NULL, &attr, GDK_WA_X | GDK_WA_Y);

  attr.window_type = GDK_WINDOW_CHILD;
  child = gdk_window_new (parent, &attr, GDK_WA_X | GDK_WA_Y);

  gtk_layout_set_static_gravity (parent, TRUE);
  gtk_layout_set_static_gravity (child, TRUE);

  gdk_window_resize (parent, 100, 110);
  gdk_window_move (parent, 0, -10);
  gdk_window_move_resize (parent, 0, 0, 100, 100);

  gdk_window_resize (parent, 100, 110);
  gdk_window_move (parent, 0, -10);
  gdk_window_move_resize (parent, 0, 0, 100, 100);

  gdk_window_get_geometry (child, NULL, &y, NULL, NULL, NULL);

  gdk_window_destroy (parent);
  gdk_window_destroy (child);
  
  return  (y == -20);
}

#endif
/* end #ifndef GTK_HAVE_FEATURES_1_1_6 */
