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

#include <Xm/CascadeBG.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/RowColumn.h>
#include "resources.h"
#include "nsMotifMenu.h"

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
    MenuCallbackProc cb = (MenuCallbackProc)call_data;
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
  cbs->mCallback = aCallback;
  cbs->mId       = aID;

  XtAddCallback(widget, XmNactivateCallback, nsXtWidget_Menu_Callback, cbs);

  return widget;

}
//-----------------------------------------------------                         
Widget CreateSeparator(Widget aParent)
{

  Widget widget = XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass,
                                          aParent,
                                          NULL);
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
    {"Debug Robot",   "R", VIEWER_DEBUGROBOT},
    {NULL, NULL, 0}
  };

  Widget menuBar;
  Widget fileMenu;
  Widget debugMenu;
  Widget menu;

  menuBar = XmCreateMenuBar(aParent, "menubar", NULL, 0);

  fileMenu   = CreatePulldownMenu(menuBar,  "File", 'F');
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

  XtManageChild(menuBar);

}
