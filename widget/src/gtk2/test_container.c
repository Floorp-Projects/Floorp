/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkwindow.h>

#include "mozcontainer.h"
#include "mozdrawingarea.h"

#include <stdio.h>

GtkWidget      *toplevel_window = NULL;
GtkWidget      *moz_container   = NULL;
MozDrawingarea *drawingarea1    = NULL;
GtkWidget      *button          = NULL;
GtkWidget      *scrollbar       = NULL;

static gint
expose_handler (GtkWidget *widget, GdkEventExpose *event);

static void
size_allocate_handler (GtkWidget *widget, GtkAllocation *allocation);

int main(int argc, char **argv)
{
  gtk_init(&argc, &argv);

  //  gdk_window_set_debug_updates(TRUE);

  toplevel_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  printf("toplevel window is %p\n", toplevel_window);

  moz_container = moz_container_new();
  printf("moz_container is %p\n", moz_container);
  gtk_signal_connect(GTK_OBJECT(moz_container), "expose_event",
                     GTK_SIGNAL_FUNC(expose_handler), NULL);
  gtk_signal_connect(GTK_OBJECT(moz_container), "size_allocate",
                     GTK_SIGNAL_FUNC(size_allocate_handler), NULL);

  gtk_container_add(GTK_CONTAINER(toplevel_window),
                    moz_container);

  gtk_widget_realize(moz_container);

  drawingarea1 = moz_drawingarea_new (NULL, MOZ_CONTAINER(moz_container));
  moz_drawingarea_set_visibility (drawingarea1, TRUE);
  moz_drawingarea_move(drawingarea1, 10, 10);

  button = gtk_button_new_with_label("foo");
  scrollbar = gtk_vscrollbar_new(NULL);

  gtk_widget_set_parent_window(button, drawingarea1->inner_window);
  gtk_widget_set_parent_window(scrollbar, drawingarea1->inner_window);

  moz_container_put(MOZ_CONTAINER(moz_container), button, 0, 0);
  moz_container_put(MOZ_CONTAINER(moz_container), scrollbar, 0, 50);

  gtk_widget_show(button); 
  gtk_widget_show(scrollbar);
  gtk_widget_show(toplevel_window);
  gtk_widget_show(moz_container);

  gtk_main();

  return 0;
}

gint
expose_handler (GtkWidget *widget, GdkEventExpose *event)
{
  printf("expose %p %p %d %d %d %d\n",
         widget,
         event->window,
         event->area.x,
         event->area.y,
         event->area.width,
         event->area.height);
  return FALSE;
}

void
size_allocate_handler (GtkWidget *widget, GtkAllocation *allocation)
{
  printf("size_allocate_handler %p %d %d %d %d\n", widget,
         allocation->x, allocation->y, allocation->width, allocation->height);
  moz_drawingarea_resize(drawingarea1, allocation->width, allocation->height);
}
