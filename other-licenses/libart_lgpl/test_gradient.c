/* test_gradient.c */

#include <math.h>
#include <gtk/gtk.h>
#include "art_point.h"
#include "art_misc.h"
#include "art_affine.h"
#include "art_svp.h"
#include "art_svp_vpath.h"
#include "art_rgb.h"
#include "art_rgb_svp.h"
#include "art_render.h"
#include "art_render_gradient.h"

GtkWidget *drawing_area;
static art_u8 *rgbdata = NULL;
int width, height;

double pos_x[2], pos_y[2];
int pos_nr = 0;

double a = 1/32.0;
double b = -1/33.00000;
double c = -1/3.0;
ArtGradientSpread spread = ART_GRADIENT_REPEAT;

ArtVpath *
randstar (int n)
{
  ArtVpath *vec;
  int i;
  double r, th;

  vec = art_new (ArtVpath, n + 2);
  for (i = 0; i < n; i++)
    {
      vec[i].code = i ? ART_LINETO : ART_MOVETO;
      r = rand () * (250.0 / RAND_MAX);
      th = i * 2 * M_PI / n;
      vec[i].x = 250 + r * cos (th);
      vec[i].y = 250 - r * sin (th);
    }
  vec[i].code = ART_LINETO;
  vec[i].x = vec[0].x;
  vec[i].y = vec[0].y;
  i++;
  vec[i].code = ART_END;
  vec[i].x = 0;
  vec[i].y = 0;
  return vec;
}

ArtVpath *
rect (void)
{
  ArtVpath *vec;
  int i;
  double r, th;

#define START 0
  
  vec = art_new (ArtVpath, 6);
  vec[0].code = ART_MOVETO;
  vec[0].x = START;
  vec[0].y = START; 
  vec[1].code = ART_LINETO;
  vec[1].x = START;
  vec[1].y = 512-START; 
  vec[2].code = ART_LINETO;
  vec[2].x = 512-START;
  vec[2].y = 512-START; 
  vec[3].code = ART_LINETO;
  vec[3].x = 512-START;
  vec[3].y = START; 
  vec[4].code = ART_LINETO;
  vec[4].x = START;
  vec[4].y = START; 
  vec[5].code = ART_END;
  vec[5].x = 0;
  vec[5].y = 0; 
  
  return vec;
}


static void
draw_test()
{
  static ArtVpath *vpath = NULL;
  static ArtSVP *svp = NULL;
  ArtRender *render;
  ArtPixMaxDepth color[3] = {0x0000, 0x0000, 0x8000 };
  ArtGradientLinear gradient;
  ArtGradientStop stops[3] = {
    { 0.02, { 0x7fff, 0x0000, 0x0000, 0x7fff }},
    { 0.5, { 0x0000, 0x0000, 0x0000, 0x1000 }},
    { 0.98, { 0x0000, 0x7fff, 0x0000, 0x7fff }}
  };

  if (!vpath) {
    vpath = rect ();
    svp = art_svp_from_vpath (vpath);
  }

  gradient.a = a;
  gradient.b = b;
  gradient.c = c;
  gradient.spread = spread;
  
  gradient.n_stops = sizeof(stops) / sizeof(stops[0]);
  gradient.stops = stops;
  
  render = art_render_new (0, 0,
			   width, height,
			   rgbdata, width * 3,
			   3, 8, ART_ALPHA_NONE,
			   NULL);
  art_render_clear_rgb (render, 0xfff0c0);
  art_render_svp (render, svp);
  art_render_gradient_linear (render, &gradient, ART_FILTER_NEAREST);
  //  art_render_image_solid (render, color);
  art_render_invoke (render);
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event( GtkWidget         *widget,
                             GdkEventConfigure *event )
{
  if (rgbdata)
    free(rgbdata);

  rgbdata = malloc(3*widget->allocation.width*widget->allocation.height);
  width = widget->allocation.width;
  height = widget->allocation.height;

  draw_test();
  
  return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint expose_event( GtkWidget      *widget,
                          GdkEventExpose *event )
{
  static GdkGC *copy_gc = NULL;

  if (copy_gc == NULL) {
    copy_gc = gdk_gc_new(widget->window);
  }

  gdk_draw_rgb_image(widget->window,
		     copy_gc,
		     event->area.x, event->area.y,
		     event->area.width, event->area.height,
		     GDK_RGB_DITHER_NONE,
		     rgbdata + event->area.x*3+event->area.y*3*width,
		     width*3 );

  return FALSE;
}

static gint button_press_event( GtkWidget      *widget,
                                GdkEventButton *event )
{
  static GdkGC *copy_gc = NULL;
  int x, y;
  
  x = event->x;
  y = event->y;

  pos_x[pos_nr] = (double) x;
  pos_y[pos_nr] = (double) y;

  pos_nr = (pos_nr+1) % 2;

  draw_test();

  if (copy_gc == NULL) {
    copy_gc = gdk_gc_new(widget->window);
  }

  gdk_draw_rgb_image(widget->window,
		     copy_gc,
		     0, 0,
		     width, height,
		     GDK_RGB_DITHER_NONE,
		     rgbdata, width*3 );

  return TRUE;
}

static gint motion_notify_event( GtkWidget *widget,
                                 GdkEventMotion *event )
{
  static GdkGC *copy_gc = NULL;
  int x, y;
  GdkModifierType state;

  if (event->is_hint) {
    gdk_window_get_pointer (event->window, &x, &y, &state);
  } else {
    x = event->x;
    y = event->y;
    state = event->state;
  }

  if (state & GDK_BUTTON1_MASK && rgbdata != NULL) {
    
    pos_x[(pos_nr+1) % 2] = (double) x;
    pos_y[(pos_nr+1) % 2] = (double) y;
    
    draw_test();
    
    if (copy_gc == NULL) {
      copy_gc = gdk_gc_new(widget->window);
    }
    
    gdk_draw_rgb_image(widget->window,
		       copy_gc,
		       0, 0,
		       width, height,
		       GDK_RGB_DITHER_NONE,
		       rgbdata, width*3 );
  }
  
  return TRUE;
}

void quit ()
{
  gtk_exit (0);
}

static void
change_gradient (GtkEntry *entry,
		 double *ptr)
{
  double d;
  d = g_ascii_strtod (gtk_entry_get_text (entry), NULL);
  if (d != 0.0)
    *ptr = 1.0/d;
  else
    *ptr = 0.0;
  draw_test();
  gtk_widget_queue_draw (drawing_area);
}

static void
change_spread (GtkEntry *entry,
	       int *ptr)
{
  double d;
  d = g_ascii_strtod (gtk_entry_get_text (entry), NULL);
  *ptr = (int)d;
  draw_test();
  gtk_widget_queue_draw (drawing_area);
}


int main( int   argc, 
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *vbox;
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  GtkWidget *button;
  GtkWidget *entry;

  gtk_init (&argc, &argv);

  gdk_rgb_init();

  pos_x[0] = 100;
  pos_y[0] = 100;
  pos_x[1] = 300;
  pos_y[1] = 300;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "Test Input");

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (quit), NULL);

  /* Create the drawing area */

  drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 512, 512);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, TRUE, TRUE, 0);

  gtk_widget_show (drawing_area);

  /* Signals used to handle backing pixmap */

  gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
		      (GtkSignalFunc) expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT(drawing_area),"configure_event",
		      (GtkSignalFunc) configure_event, NULL);

  /* Event signals */

  gtk_signal_connect (GTK_OBJECT (drawing_area), "motion_notify_event",
		      (GtkSignalFunc) motion_notify_event, NULL);
  gtk_signal_connect (GTK_OBJECT (drawing_area), "button_press_event",
		      (GtkSignalFunc) button_press_event, NULL);

  gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_POINTER_MOTION_MASK
			 | GDK_POINTER_MOTION_HINT_MASK);

  
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (entry), "activate",
		      GTK_SIGNAL_FUNC (change_gradient),
		      &a);
  gtk_entry_set_text  (GTK_ENTRY (entry),
		       g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE,
				       1.0/a));
  gtk_widget_show (entry);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (entry), "activate",
		      GTK_SIGNAL_FUNC (change_gradient),
		      &b);
  gtk_entry_set_text  (GTK_ENTRY (entry),
		       g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE,
				       1.0/b));
  gtk_widget_show (entry);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (entry), "activate",
		      GTK_SIGNAL_FUNC (change_gradient),
		      &c);
  gtk_entry_set_text  (GTK_ENTRY (entry),
		       g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE,
				       1.0/c));
  gtk_widget_show (entry);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (entry), "activate",
		      GTK_SIGNAL_FUNC (change_spread),
		      &spread);
  gtk_entry_set_text  (GTK_ENTRY (entry),
		       g_strdup_printf ("%d", spread));
  gtk_widget_show (entry);

  
  /* .. And a quit button */
  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
