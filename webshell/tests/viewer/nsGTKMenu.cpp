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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <gtk/gtk.h>
#include "resources.h"

#include "stdio.h"

void gtk_ifactory_cb (gpointer callback_data,
	guint callback_action, 
	GtkWidget *widget)
{
  g_message ("ItemFactory: activated \"%s\"",
    gtk_item_factory_path_from_widget(widget));
}

GtkItemFactoryEntry menu_items[] =
{
  { "/_File",            NULL,	gtk_ifactory_cb,	0,	"<Branch>" },
  { "/File/_New Window", NULL,	gtk_ifactory_cb,	0	},
  { "/File/_Open...",	 NULL,	gtk_ifactory_cb,	0 },
  { "/File/_Samples",	 NULL,	gtk_ifactory_cb,	0,	"<Branch>" },
  { "/File/Samples/demo #0",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #1",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #2",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #3",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #4",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #5",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #6",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #7",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #8",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #9",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #10",	 NULL,	gtk_ifactory_cb,	0, },
  { "/File/Samples/Top 100 Sites",	 NULL,	gtk_ifactory_cb,	0, },

  { "/_Edit", NULL,	gtk_ifactory_cb,	0,	"<Branch>"	},
  { "/Edit/Cut",	"T",	gtk_ifactory_cb,	0 },
  { "/Edit/Copy",	"C",	gtk_ifactory_cb,	0 },
  { "/Edit/Paste",	"P",	gtk_ifactory_cb,	0 },
  { "/Edit/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Edit/Select All",	"A",	gtk_ifactory_cb,	0 },
  { "/Edit/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Edit/Find in Page",	"F",	gtk_ifactory_cb,	0 },

  { "/_Debug", NULL,	gtk_ifactory_cb,	0,	"<Branch>"	},
  { "/Debug/Visual Debugging",	"V",	gtk_ifactory_cb,	0 },
  { "/Debug/Reflow Test",	"R",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Dump Content",	"C",	gtk_ifactory_cb,	0 },
  { "/Debug/Dump Frames",	"F",	gtk_ifactory_cb,	0 },
  { "/Debug/Dump Views",	"V",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Dump Style Sheets",	"S",	gtk_ifactory_cb,	0 },
  { "/Debug/Dump Style Contexts",	"T",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Show Content Size",	"z",	gtk_ifactory_cb,	0 },
  { "/Debug/Show Frame Size",	"a",	gtk_ifactory_cb,	0 },
  { "/Debug/Show Style Size",	"y",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Debug Save",	"v",	gtk_ifactory_cb,	0 },
  { "/Debug/Debug Toggle Selection",	"q",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Debug Debug Robot",	"R",	gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	NULL,	gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Show Content Quality",	".",	gtk_ifactory_cb,	0 },
};

void CreateViewerMenus(GtkWidget *aParent, gpointer aCallback) 
{
  GtkItemFactory *item_factory;
  int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", NULL);
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

  gtk_box_pack_start (GTK_BOX (aParent),
	gtk_item_factory_get_widget (item_factory,
	"<main>"),
	FALSE, FALSE, 0);
							    
/*
  menu = CreatePulldownMenu(fileMenu, "Print Preview", 'P');
  CreateMenuItem(menu, "One Column", VIEWER_ONE_COLUMN, aCallback);
  CreateMenuItem(menu, "Two Column", VIEWER_TWO_COLUMN, aCallback);
  CreateMenuItem(menu, "Three Column", VIEWER_THREE_COLUMN, aCallback);

  CreateMenuItem(fileMenu, "Exit", VIEWER_EXIT, aCallback);

  menu = CreatePulldownMenu(menuBar,  "Tools", 'T');
  CreateMenuItem(menu, "Java Script Console", JS_CONSOLE, aCallback);
  CreateMenuItem(menu, "Editor Mode", EDITOR_MODE, aCallback);

  XtManageChild(menuBar);
*/
}
