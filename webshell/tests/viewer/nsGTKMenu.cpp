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

#include "nsBrowserWindow.h"
#include "resources.h"
#include "nscore.h"

#include "stdio.h"

typedef GtkItemFactoryCallback GIFC;

void gtk_ifactory_cb (nsBrowserWindow *nbw,
	guint callback_action, 
	GtkWidget *widget)
{
  nbw->DispatchMenuItem(callback_action);
}

GtkItemFactoryEntry menu_items[] =
{
  { "/_File",				nsnull,	nsnull,			0,			"<Branch>" },
  { "/File/_New Window",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_WINDOW_OPEN,	nsnull },
  { "/File/_Open...",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_FILE_OPEN,	nsnull },
  { "/File/_View Source",		nsnull, (GIFC)gtk_ifactory_cb,	VIEW_SOURCE,		nsnull },
  { "/File/_Samples",			nsnull,	nsnull,			0,			"<Branch>" },
  { "/File/Samples/demo #0",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO0,		nsnull },
  { "/File/Samples/demo #1",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO1,		nsnull },
  { "/File/Samples/demo #2",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO2,		nsnull },
  { "/File/Samples/demo #3",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO3,		nsnull },
  { "/File/Samples/demo #4",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO4,		nsnull },
  { "/File/Samples/demo #5",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO5,		nsnull },
  { "/File/Samples/demo #6",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO6,		nsnull },
  { "/File/Samples/demo #7",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO7,		nsnull },
  { "/File/Samples/demo #8",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO8,		nsnull },
  { "/File/Samples/demo #9",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO9,		nsnull },
  { "/File/Samples/demo #10",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO10,		nsnull },
  { "/File/Samples/demo #11",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO11,		nsnull },
  { "/File/Samples/demo #12",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO12,		nsnull },
  { "/File/Samples/demo #13",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO13,		nsnull },
  { "/File/_Test Sites",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOP100,		nsnull },
  { "/File/XPToolkit Tests",		nsnull,	nsnull,			0,			"<Branch>" },
  { "/File/XPToolkit Tests/Toolbar Test 1",       nsnull, (GIFC)gtk_ifactory_cb,  VIEWER_XPTOOLKITTOOLBAR1,     nsnull },
  { "/File/XPToolkit Tests/Tree Test 1",       nsnull, (GIFC)gtk_ifactory_cb,  VIEWER_XPTOOLKITTREE1,     nsnull },
  { "/File/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/File/Print Preview",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_ONE_COLUMN,	nsnull },
  { "/File/Print",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_PRINT,		nsnull },
  { "/File/Print Setup",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_PRINT_SETUP,	nsnull },
  { "/File/sep2",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/File/_Exit",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EXIT,		nsnull },

  { "/_Edit",				nsnull,	nsnull,			0,			"<Branch>" },
  { "/Edit/Cu_t",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EDIT_CUT,	nsnull },
  { "/Edit/_Copy",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EDIT_COPY,	nsnull },
  { "/Edit/_Paste",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EDIT_PASTE,	nsnull },
  { "/Edit/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Edit/Select All",			nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EDIT_SELECTALL,	nsnull },
  { "/Edit/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Edit/Find in Page",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_EDIT_FINDINPAGE,	nsnull },

#ifdef DEBUG
  { "/_Debug",				nsnull,	nsnull,			0,			"<Branch>"	},
  { "/Debug/_Visual Debugging",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_VISUAL_DEBUGGING,nsnull },
  { "/Debug/_Reflow Test",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_REFLOW_TEST,	nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Dump _Content",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DUMP_CONTENT,	nsnull },
  { "/Debug/Dump _Frames",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DUMP_FRAMES,	nsnull },
  { "/Debug/Dump _Views",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DUMP_VIEWS,	nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Dump _Style Sheets",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DUMP_STYLE_SHEETS,	nsnull },
  { "/Debug/Dump _Style Contexts",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DUMP_STYLE_CONTEXTS,	nsnull},
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Show Content Size",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SHOW_CONTENT_SIZE,nsnull },
  { "/Debug/Show Frame Size",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SHOW_FRAME_SIZE,	nsnull },
  { "/Debug/Show Style Size",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SHOW_STYLE_SIZE,	nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Debug Save",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEBUGSAVE,	nsnull },
  { "/Debug/Debug Output Text",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DISPLAYTEXT,	nsnull },
  { "/Debug/Debug Output HTML",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DISPLAYHTML,	nsnull },
  { "/Debug/Debug Toggle Selection",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_SELECTION,nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Debug Robot",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEBUGROBOT,	nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Show Content Quality",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SHOW_CONTENT_QUALITY,	nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Style",	       nsnull, nsnull,        0,        "<Branch>" },
  { "/Debug/Style/Select _Style Sheet",	       nsnull, nsnull,        0,        "<Branch>" },
  { "/Debug/Style/Select Style Sheet/List Available Sheets", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_LIST, nsnull },
  { "/Debug/Style/Select Style Sheet/sep1", nsnull, nsnull,        0,        "<Separator>" },
  { "/Debug/Style/Select Style Sheet/Select Default", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_DEFAULT, nsnull },
  { "/Debug/Style/Select Style Sheet/sep1", nsnull, nsnull,        0,        "<Separator>" },
  { "/Debug/Style/Select Style Sheet/Select Alternative 1", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_ONE, nsnull },
  { "/Debug/Style/Select Style Sheet/Select Alternative 2", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_TWO, nsnull },
  { "/Debug/Style/Select Style Sheet/Select Alternative 3", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_THREE, nsnull },
  { "/Debug/Style/Select Style Sheet/Select Alternative 4", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_FOUR, nsnull },
  { "/Debug/Style/_Compatibility Mode",		nsnull,	nsnull,			0,			"<Branch>" },
  { "/Debug/Style/Compatibility Mode/Nav Quirks",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_NAV_QUIRKS_MODE,	nsnull },
  { "/Debug/Style/Compatibility Mode/Standard",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_STANDARD_MODE,	nsnull },

  { "/Debug/Style/_Widget Render Mode",		nsnull,	nsnull,			0,			"<Branch>" },
  { "/Debug/Style/Widget Render Mode/Native",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_NATIVE_WIDGET_MODE,	nsnull },
  { "/Debug/Style/Widget Render Mode/Gfx",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_GFX_WIDGET_MODE,	nsnull },
#endif

  { "/_Tools",                                nsnull, nsnull,                 0,              "<Branch>" },
  { "/Tools/_JavaScript Console",	nsnull,	(GIFC)gtk_ifactory_cb,	JS_CONSOLE,	nsnull },
  { "/Tools/_Editor Mode",		nsnull,	(GIFC)gtk_ifactory_cb,	EDITOR_MODE,	nsnull }
};

void CreateViewerMenus(GtkWidget *aParent, gpointer data) 
{
  GtkItemFactory *item_factory;
  GtkWidget *menubar;
  
  int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", nsnull);
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, data);
/* HACK HACK HACK */
  menubar = gtk_item_factory_get_widget (item_factory, "<main>");
  gtk_widget_show(menubar);

  gtk_menu_bar_set_shadow_type (GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);

  gtk_box_pack_start(GTK_BOX(aParent->parent), menubar, PR_FALSE, PR_FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(aParent->parent), menubar, 0);
}
