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

#include <Xm/CascadeBG.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/RowColumn.h>
#include "resources.h"
#include "nsMotifMenu.h"
#include "nsError.h" // For NS_ERROR_OUT_OF_MEMORY.

#include "stdio.h"

//==============================================================
  typedef struct _callBackInfo {
    MenuCallbackProc mCallback;
    PRUint32         mId;
  } MenuCallbackStruct;

//==============================================================
void nsXtWidget_Menu_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  MenuCallbackStruct * cbs = (MenuCallbackStruct *)p;
  if (cbs != NULL) {
    if (cbs->mCallback != NULL) {
      (*cbs->mCallback)(cbs->mId);
    }
  }
}

//-----------------------------------------------------
Widget CreatePulldownMenu(Widget   aParent, 
                          char   * aMenuTitle,
                          char     aMenuMnemonic)
{

  Widget pullDown;
  Widget casBtn;
  XmString str;

  pullDown = XmCreatePulldownMenu(aParent, "_pulldown", NULL, 0);

  str = XmStringCreateLocalized(aMenuTitle);
  casBtn = XtVaCreateManagedWidget(aMenuTitle, 
                                   xmCascadeButtonGadgetClass, aParent,
                                   XmNsubMenuId, pullDown,
                                   XmNlabelString, str,
                                   XmNmnemonic, aMenuMnemonic,
                                   NULL);
  XmStringFree(str);

  return pullDown;

}

//-----------------------------------------------------                         
Widget CreateMenuItem(Widget          aParent, 
                      char *          aTitle,
                      long            aID,
                      MenuCallbackProc aCallback)
{

  Widget widget = XtVaCreateManagedWidget(aTitle, xmPushButtonGadgetClass,
                                          aParent,
                                          NULL);

  MenuCallbackStruct * cbs = new MenuCallbackStruct();
  if (cbs) {
    cbs->mCallback = aCallback;
    cbs->mId       = aID;
  }
  else cbs = (MenuCallbackStruct *)NS_ERROR_OUT_OF_MEMORY;

  XtAddCallback(widget, XmNactivateCallback, nsXtWidget_Menu_Callback, cbs);

  return widget;

}
//-----------------------------------------------------                         
Widget CreateSeparator(Widget aParent)
{

  Widget widget = XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass,
                                          aParent,
                                          NULL);
  return widget;
}

typedef struct _menuBtns {
  char * title;
  char * mneu;
  long   command;
} MenuBtns;

//-----------------------------------------------------
void AddMenuItems(Widget             aParent, 
                  MenuBtns *         aBtns, 
                  MenuCallbackProc aCallback) 
{
  int i = 0;
  while (aBtns[i].title != NULL) {
    if (!strcmp(aBtns[i].title, "separator")) {
      CreateSeparator(aParent);
    } else {
      CreateMenuItem(aParent, aBtns[i].title, aBtns[i].command, aCallback);
    }
    i++;
  }
}

//-----------------------------------------------------
void CreateViewerMenus(Widget aParent, MenuCallbackProc aCallback) 
{
  MenuBtns editBtns[] = {
    {"Cut", "T", VIEWER_EDIT_CUT},
    {"Copy", "C", VIEWER_EDIT_COPY},
    {"Paste", "P", VIEWER_EDIT_PASTE},
    {"separator", NULL, 0},
    {"Select All", "A", VIEWER_EDIT_SELECTALL},
    {"separator", NULL, 0},
    {"Find in Page", "F", VIEWER_EDIT_FINDINPAGE},
    {NULL, NULL, 0}
  };

  MenuBtns  debugBtns[] = {
    {"Visual Debugging", "V", VIEWER_VISUAL_DEBUGGING},
    {"Reflow Test", "R", VIEWER_REFLOW_TEST},
    {"separator", NULL, 0},
    {"Dump Content", "C", VIEWER_DUMP_CONTENT},
    {"Dump Frames",  "F", VIEWER_DUMP_FRAMES},
    {"Dump Views",   "V", VIEWER_DUMP_VIEWS},
    {"separator", NULL, 0},
    {"Dump Style Sheets",   "S", VIEWER_DUMP_STYLE_SHEETS},
    {"Dump Style Contexts",   "T", VIEWER_DUMP_STYLE_CONTEXTS},
    {"separator", NULL, 0},
    {"Show Content Size",   "z", VIEWER_SHOW_CONTENT_SIZE},
    {"Show Frame Size",   "a", VIEWER_SHOW_FRAME_SIZE},
    {"Show Style Size",   "y", VIEWER_SHOW_STYLE_SIZE},
    {"separator", NULL, 0},
    {"Debug Save",   "v", VIEWER_DEBUGSAVE},
    {"Debug Toggle Selection",   "q", VIEWER_TOGGLE_SELECTION},
    {"separator", NULL, 0},
    {"Debug Robot",   "R", VIEWER_DEBUGROBOT},
    {"separator", NULL, 0},
    {"Show Content Quality",   ".", VIEWER_SHOW_CONTENT_QUALITY},
    {NULL, NULL, 0}
  };

  Widget menuBar;
  Widget fileMenu;
  Widget debugMenu;
  Widget menu;

  menuBar = XmCreateMenuBar(aParent, "menubar", NULL, 0);

  fileMenu   = CreatePulldownMenu(menuBar,  "File", 'F');
  CreateMenuItem(fileMenu, "New Window", VIEWER_WINDOW_OPEN, aCallback);
  CreateMenuItem(fileMenu, "Open...", VIEWER_FILE_OPEN, aCallback);

  menu = CreatePulldownMenu(fileMenu, "Samples", 'S');
  CreateMenuItem(menu, "demo #0", VIEWER_DEMO0, aCallback);
  CreateMenuItem(menu, "demo #1", VIEWER_DEMO1, aCallback);
  CreateMenuItem(menu, "demo #2", VIEWER_DEMO2, aCallback);
  CreateMenuItem(menu, "demo #3", VIEWER_DEMO3, aCallback);
  CreateMenuItem(menu, "demo #4", VIEWER_DEMO4, aCallback);
  CreateMenuItem(menu, "demo #5", VIEWER_DEMO5, aCallback);
  CreateMenuItem(menu, "demo #6", VIEWER_DEMO6, aCallback);
  CreateMenuItem(menu, "demo #7", VIEWER_DEMO7, aCallback);
  CreateMenuItem(menu, "demo #8", VIEWER_DEMO8, aCallback);
  CreateMenuItem(menu, "demo #9", VIEWER_DEMO9, aCallback);
  CreateMenuItem(menu, "demo #10", VIEWER_DEMO10, aCallback);
  CreateMenuItem(menu, "demo #11", VIEWER_DEMO11, aCallback);
  CreateMenuItem(menu, "demo #12", VIEWER_DEMO12, aCallback);

  CreateMenuItem(fileMenu, "Top 100 Sites", VIEWER_TOP100, aCallback);

  menu = CreatePulldownMenu(fileMenu, "Print Preview", 'P');
  CreateMenuItem(menu, "One Column", VIEWER_ONE_COLUMN, aCallback);
  CreateMenuItem(menu, "Two Column", VIEWER_TWO_COLUMN, aCallback);
  CreateMenuItem(menu, "Three Column", VIEWER_THREE_COLUMN, aCallback);

  CreateMenuItem(fileMenu, "Exit", VIEWER_EXIT, aCallback);

  menu = CreatePulldownMenu(menuBar,  "Edit", 'E');
  AddMenuItems(menu, editBtns, aCallback);

  debugMenu = CreatePulldownMenu(menuBar,  "Debug", 'D');
  AddMenuItems(debugMenu, debugBtns, aCallback);

  menu = CreatePulldownMenu(menuBar,  "Tools", 'T');
  CreateMenuItem(menu, "Java Script Console", JS_CONSOLE, aCallback);
  CreateMenuItem(menu, "Editor Mode", EDITOR_MODE, aCallback);

  XtManageChild(menuBar);

}
