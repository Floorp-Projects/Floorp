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

#ifndef GTK_HAVE_FEATURES_1_1_6
#include "gtklayout.h"
#endif

#include "resources.h"
#include "nscore.h"

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
  { "/_File",            nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0,	"<Branch>" },
  { "/File/_New Window", nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0	},
  { "/File/_Open...",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/File/_Samples",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0,	"<Branch>" },
  { "/File/Samples/demo #0",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #1",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #2",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #3",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #4",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #5",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #6",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #7",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #8",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #9",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/demo #10",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },
  { "/File/Samples/Top 100 Sites",	 nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, },

  { "/_Edit", nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0,	"<Branch>"	},
  { "/Edit/Cut",	"T",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Edit/Copy",	"C",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Edit/Paste",	"P",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Edit/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Edit/Select All",	"A",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Edit/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Edit/Find in Page",	"F",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },

  { "/_Debug", nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0,	"<Branch>"	},
  { "/Debug/Visual Debugging",	"V",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Reflow Test",	"R",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Dump Content",	"C",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Dump Frames",	"F",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Dump Views",	"V",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Dump Style Sheets",	"S",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Dump Style Contexts",	"T",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Show Content Size",	"z",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Show Frame Size",	"a",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Show Style Size",	"y",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Debug Save",	"v",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/Debug Toggle Selection",	"q",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Debug Debug Robot",	"R",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
  { "/Debug/sep1",	nsnull,	(GtkItemFactoryCallback)gtk_ifactory_cb,	0, "<Separator>" },
  { "/Debug/Show Content Quality",	".",	(GtkItemFactoryCallback)gtk_ifactory_cb,	0 },
};

void CreateViewerMenus(GtkWidget *aParent, gpointer aCallback) 
{
  GtkItemFactory *item_factory;
  int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", nsnull);
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, nsnull);
/*
  gtk_box_pack_start (GTK_BOX (aParent),
	gtk_item_factory_get_widget (item_factory,
	"<main>"),
	FALSE, FALSE, 0);
*/							    
  gtk_layout_put (GTK_LAYOUT (aParent),
	gtk_item_factory_get_widget (item_factory, "<main>"),
	0, 0);
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
