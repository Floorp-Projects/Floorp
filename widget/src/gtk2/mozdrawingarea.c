#include "mozdrawingarea.h"

/* init methods */
static void moz_drawingarea_class_init          (MozDrawingareaClass *klass);
static void moz_drawingarea_init                (MozDrawingarea *drawingarea);

/* static methods */
static void moz_drawingarea_create_windows      (MozDrawingarea *drawingarea,
						 GdkWindow *parent, GtkWidget *widget);

static GObjectClass *parent_class = NULL;

GtkType
moz_drawingarea_get_type(void)
{
  static GtkType moz_drawingarea_type = 0;

  if (!moz_drawingarea_type)
    {
      static GTypeInfo moz_drawingarea_info = {
	sizeof(MozDrawingareaClass), /* class size */
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) moz_drawingarea_class_init, /* class_init */
	NULL, /* class_destroy */
	NULL, /* class_data */
	sizeof(MozDrawingarea), /* instance_size */
	0, /* n_preallocs */
	(GInstanceInitFunc) moz_drawingarea_init, /* instance_init */
	NULL, /* value_table */
      };

      moz_drawingarea_type = g_type_register_static (G_TYPE_OBJECT,
						     "MozDrawingarea",
						     &moz_drawingarea_info, 0);
    }

  return moz_drawingarea_type;
}

MozDrawingarea *
moz_drawingarea_new (MozDrawingarea *parent, MozContainer *widget_parent)
{
  MozDrawingarea *drawingarea;

  drawingarea = g_object_new(MOZ_DRAWINGAREA_TYPE, NULL);

  drawingarea->parent = parent;

  if (!parent)
    moz_drawingarea_create_windows(drawingarea,
				   GTK_WIDGET(widget_parent)->window,
				   GTK_WIDGET(widget_parent));
  else
    moz_drawingarea_create_windows(drawingarea,
				   parent->inner_window, 
				   GTK_WIDGET(widget_parent));

  return drawingarea;
}

void
moz_drawingarea_class_init (MozDrawingareaClass *klass)
{
  parent_class = g_type_class_peek_parent(klass);
}

void
moz_drawingarea_init (MozDrawingarea *drawingarea)
{
  
}

void
moz_drawingarea_create_windows (MozDrawingarea *drawingarea, GdkWindow *parent,
				GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint          attributes_mask = 0;
  
  /* create the clipping window */
  attributes.event_mask = (GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK);
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = 1;
  attributes.height = 1;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  attributes_mask |= GDK_WA_VISUAL | GDK_WA_COLORMAP |
    GDK_WA_X | GDK_WA_Y;

  drawingarea->clip_window = gdk_window_new (parent, &attributes,
					     attributes_mask);
  gdk_window_set_user_data(drawingarea->clip_window, widget);

  /* set the default pixmap to None so that you don't end up with the
     gtk default which is BlackPixel. */
  gdk_window_set_back_pixmap(drawingarea->clip_window, NULL, FALSE);

  /* create the inner window */
  drawingarea->inner_window = gdk_window_new (drawingarea->clip_window,
					      &attributes, attributes_mask);
  gdk_window_set_user_data(drawingarea->inner_window, widget);

  /* set the default pixmap to None so that you don't end up with the
     gtk default which is BlackPixel. */
  gdk_window_set_back_pixmap(drawingarea->inner_window, NULL, FALSE);
}

void
moz_drawingarea_move (MozDrawingarea *drawingarea,
		      gint x, gint y)
{
  gdk_window_move(drawingarea->clip_window, x, y);
}

void
moz_drawingarea_resize (MozDrawingarea *drawingarea,
			gint width, gint height)
{
  gdk_window_resize(drawingarea->clip_window, width, height);
  gdk_window_resize(drawingarea->inner_window, width, height);
}

void
moz_drawingarea_move_resize (MozDrawingarea *drawingarea,
			     gint x, gint y, gint width, gint height)
{
  gdk_window_resize(drawingarea->inner_window, width, height);
  gdk_window_move_resize(drawingarea->clip_window, x, y, width, height);
}

void
moz_drawingarea_set_visibility (MozDrawingarea *drawingarea,
				gboolean visibility)
{
  if (visibility)
    {
      gdk_window_show(drawingarea->inner_window);
      gdk_window_show(drawingarea->clip_window);
    }
  else
    {
      gdk_window_hide(drawingarea->clip_window);
      gdk_window_hide(drawingarea->inner_window);
    }
}
