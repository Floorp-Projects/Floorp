/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <gtk/gtk.h>

#include "gdksuperwin.h"
#include "gtkmozbox.h"

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
  { "/File/Samples/demo #14",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO14,		nsnull },
  { "/File/Samples/demo #15",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO15,		nsnull },
  { "/File/Samples/demo #16",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO16,		nsnull },
  { "/File/Samples/demo #17",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEMO17,		nsnull },
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

//#ifdef DEBUG // turning off for now
  { "/_Debug",				nsnull,	nsnull,			0,			"<Branch>"	},
  { "/Debug/_Visual Debugging",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_VISUAL_DEBUGGING,nsnull },
  { "/Debug/Visual _Event Debugging",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_VISUAL_EVENT_DEBUGGING,nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Event Debugging/Toggle Paint Flashing",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_PAINT_FLASHING,nsnull },
  { "/Debug/Event Debugging/Toggle Paint Dumping",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_PAINT_DUMPING,nsnull },
  { "/Debug/Event Debugging/Toggle Invalidate Dumping",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_INVALIDATE_DUMPING,nsnull },
  { "/Debug/Event Debugging/Toggle Event Dumping",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_EVENT_DUMPING,nsnull },
  { "/Debug/Event Debugging/sep1", nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Event Debugging/Toggle Motion Event Dumping",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_MOTION_EVENT_DUMPING,nsnull },
  { "/Debug/Event Debugging/Toggle Crossing Event Dumping",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_TOGGLE_CROSSING_EVENT_DUMPING,nsnull },
  { "/Debug/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/Debug/Reflow Debugging/_Reflow Test",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_REFLOW_TEST,	nsnull },
  { "/Debug/Reflow Debugging/Dump Reflow Stats",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DEBUG_DUMP_REFLOW_TOTS,	nsnull },
  { "/Debug/Reflow Debugging/Reflow Counts On",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DSP_REFLOW_CNTS_ON,	nsnull },
  { "/Debug/Reflow Debugging/Reflow Counts Off",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_DSP_REFLOW_CNTS_OFF,	nsnull },
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
  { "/_Style",				nsnull,	nsnull,			0,			"<Branch>"	},
  { "/Style/Select _Style Sheet",	       nsnull, nsnull,        0,        "<Branch>" },
  { "/Style/Select Style Sheet/List Available Sheets", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_LIST, nsnull },
  { "/Style/Select Style Sheet/sep1", nsnull, nsnull,        0,        "<Separator>" },
  { "/Style/Select Style Sheet/Select Default", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_DEFAULT, nsnull },
  { "/Style/Select Style Sheet/sep1", nsnull, nsnull,        0,        "<Separator>" },
  { "/Style/Select Style Sheet/Select Alternative 1", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_ONE, nsnull },
  { "/Style/Select Style Sheet/Select Alternative 2", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_TWO, nsnull },
  { "/Style/Select Style Sheet/Select Alternative 3", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_THREE, nsnull },
  { "/Style/Select Style Sheet/Select Alternative 4", nsnull, (GIFC)gtk_ifactory_cb, VIEWER_SELECT_STYLE_FOUR, nsnull },
  { "/Style/_Compatibility Mode",		nsnull,	nsnull,			0,			"<Branch>" },
  { "/Style/Compatibility Mode/Use DTD",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_USE_DTD_MODE,	nsnull },
  { "/Style/Compatibility Mode/Nav Quirks",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_NAV_QUIRKS_MODE,	nsnull },
  { "/Style/Compatibility Mode/Standard",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_STANDARD_MODE,	nsnull },
  { "/Style/_Gfx Scrollbars Mode",		nsnull,	nsnull,			0,			"<Branch>" },
  { "/Style/Gfx Scrollbars Mode/Gfx On",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_GFX_SCROLLBARS_ON,	nsnull },
  { "/Style/Gfx Scrollbars Mode/Gfx Off",	nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_GFX_SCROLLBARS_OFF,	nsnull },
//#endif

  { "/_URLs",                                nsnull, nsnull,                 0,              "<Branch>" },
  { "/URLs/Goto URL 1",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_GOTO_TEST_URL1,nsnull },
  { "/URLs/Goto URL 2",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_GOTO_TEST_URL2,nsnull },
  { "/URLs/sep1",			nsnull,	nsnull,			0,			"<Separator>" },
  { "/URLs/Save URL 1",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SAVE_TEST_URL1,nsnull },
  { "/URLs/Save URL 2",		nsnull,	(GIFC)gtk_ifactory_cb,	VIEWER_SAVE_TEST_URL2,nsnull },

  { "/_Tools",                                nsnull, nsnull,                 0,              "<Branch>" },
  { "/Tools/_JavaScript Console",	nsnull,	(GIFC)gtk_ifactory_cb,	JS_CONSOLE,	nsnull },
  { "/Tools/_Editor Mode",		nsnull,	(GIFC)gtk_ifactory_cb,	EDITOR_MODE,	nsnull }
};

void CreateViewerMenus(nsIWidget *   aParent, 
                       gpointer      data,
                       GtkWidget **  aMenuBarOut) 
{
  NS_ASSERTION(nsnull != aParent,"null parent.");
  NS_ASSERTION(nsnull != aMenuBarOut,"null out param.");

  GtkItemFactory *item_factory;
  GtkWidget *menubar;

  GdkSuperWin *gdkSuperWin;
  GtkWidget   *mozBox;

  gdkSuperWin = (GdkSuperWin*)aParent->GetNativeData(NS_NATIVE_WIDGET);
  
  int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", nsnull);

  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, data);

  menubar = gtk_item_factory_get_widget (item_factory, "<main>");

  gtk_menu_bar_set_shadow_type (GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);

  NS_ASSERTION(GDK_IS_SUPERWIN(gdkSuperWin), "code assumes a gdksuperwin.");
  mozBox = gtk_mozbox_new(gdkSuperWin->bin_window);
  NS_ASSERTION((mozBox != NULL), "failed to create mozBox.");

  gtk_container_add(GTK_CONTAINER(mozBox), menubar);
  gtk_mozbox_set_position(GTK_MOZBOX(mozBox), 0, 0 );
  gtk_widget_show(menubar);
  gtk_widget_show(mozBox);

  *aMenuBarOut = menubar;
}
