/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * Copyright (C) 1999 Alexander Larsson.  All Rights Reserved.
 */
#include <gtk/gtk.h>
#include <gtkmozilla.h>

struct browser {
  GtkMozilla *mozilla;
  GtkWidget *window;
  GtkWidget *back_button;
  GtkWidget *forward_button;
  GtkWidget *content_button;
  GtkWidget *index_button;
  GtkWidget *search_button;
};

static void
set_buttons(struct browser *browser)
{
  gtk_widget_set_sensitive(browser->back_button,
			   gtk_mozilla_can_back(browser->mozilla));
  gtk_widget_set_sensitive(browser->forward_button,
			   gtk_mozilla_can_forward(browser->mozilla));
}

static void begin_load_url(GtkWidget *mozilla,
			   const char *url,
			   struct browser *browser)
{
  printf("begin_load_url(%s)\n", url);
  set_buttons(browser);
}

static gint configure_event_after(GtkWidget *window,
				  GdkEvent  *event,
				  GtkMozilla *moz)
{
  GtkAllocation *alloc;

  alloc =  &GTK_WIDGET(moz)->allocation;
  gtk_mozilla_resize(moz, alloc->width, alloc->height);
  return 1;
}

static void back_clicked(GtkWidget *button,
			 gpointer arg)
{
  GtkMozilla *moz = GTK_MOZILLA(arg);
  gtk_mozilla_back(moz);
}

static void forward_clicked(GtkWidget *button,
			    gpointer arg)
{
  GtkMozilla *moz = GTK_MOZILLA(arg);
  gtk_mozilla_forward(moz);
}

static void content_clicked(GtkWidget *button,
			    gpointer arg)
{
  GtkMozilla *moz = GTK_MOZILLA(arg);
  printf("index_clicked()\n");
  printf("loading: " "file://"  HELPDATADIR "/content.html\n");
  gtk_mozilla_load_url(moz, "file://" HELPDATADIR "/content.html");
}

static void index_clicked(GtkWidget *button,
			  gpointer arg)
{
  GtkMozilla *moz = GTK_MOZILLA(arg);
  printf("index_clicked()\n");
  printf("loading: " "file://"  HELPDATADIR "/index.html\n");
  gtk_mozilla_load_url(moz, "file://" HELPDATADIR "/index.html");
}

static void search_clicked(GtkWidget *button,
			    gpointer arg)
{
  GtkMozilla *moz = GTK_MOZILLA(arg);
}

int main( int   argc,
          char *argv[] )
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *back_button;
  GtkWidget *forward_button;
  GtkWidget *content_button;
  GtkWidget *index_button;
  GtkWidget *search_button;
  GtkWidget *mozilla;
  struct browser browser;

  /* Initialise GTK */
  gtk_set_locale ();

  gtk_init (&argc, &argv);

  gdk_rgb_init();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size( GTK_WINDOW(window), 500, 400);
  gtk_window_set_title(GTK_WINDOW(window), "Help Browser");
  
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
                      GTK_SIGNAL_FUNC(gtk_main_quit),
                      NULL);
  
  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show(vbox);
  
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  back_button = gtk_button_new_with_label("Back");
  gtk_box_pack_start (GTK_BOX (hbox), back_button, FALSE, FALSE, 0);
  gtk_widget_show(back_button);

  forward_button = gtk_button_new_with_label("Forward");
  gtk_box_pack_start (GTK_BOX (hbox), forward_button, FALSE, FALSE, 0);
  gtk_widget_show(forward_button);
  
  content_button = gtk_button_new_with_label("Content");
  gtk_box_pack_start (GTK_BOX (hbox), content_button, FALSE, FALSE, 0);
  gtk_widget_show(content_button);
  
  index_button = gtk_button_new_with_label("Index");
  gtk_box_pack_start (GTK_BOX (hbox), index_button, FALSE, FALSE, 0);
  gtk_widget_show(index_button);
  
  search_button = gtk_button_new_with_label("Search");
  gtk_box_pack_start (GTK_BOX (hbox), search_button, FALSE, FALSE, 0);
  gtk_widget_show(search_button);
  
  gtk_widget_show(hbox);

  mozilla = gtk_mozilla_new();
  gtk_box_pack_start (GTK_BOX (vbox), mozilla, TRUE, TRUE, 0);
  gtk_widget_show_all(mozilla);
  
  gtk_widget_show(window);

  browser.mozilla = GTK_MOZILLA(mozilla);
  browser.window = window;
  browser.back_button = back_button;
  browser.forward_button = forward_button;
  browser.content_button = content_button;
  browser.index_button = index_button;
  browser.search_button = search_button;
  
  gtk_signal_connect_after(GTK_OBJECT (window), "configure_event",
			   (GtkSignalFunc) configure_event_after,
			   mozilla);

  gtk_signal_connect(GTK_OBJECT (back_button), "clicked",
		     (GtkSignalFunc) back_clicked,
		     mozilla);

  gtk_signal_connect(GTK_OBJECT (forward_button), "clicked",
		     (GtkSignalFunc) forward_clicked,
		     mozilla);

  gtk_signal_connect(GTK_OBJECT (mozilla), "begin_load_url",
		     (GtkSignalFunc) begin_load_url,
		     &browser);

  gtk_signal_connect(GTK_OBJECT (content_button), "clicked",
		     (GtkSignalFunc) content_clicked,
		     mozilla);

  gtk_signal_connect(GTK_OBJECT (index_button), "clicked",
		     (GtkSignalFunc) index_clicked,
		     mozilla);

  gtk_signal_connect(GTK_OBJECT (search_button), "clicked",
		     (GtkSignalFunc) search_clicked,
		     mozilla);

  set_buttons(&browser);
  
  gtk_main ();
  
  return(0);
}






