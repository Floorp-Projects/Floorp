/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#include <qwidget.h>

#include "nsBrowserWindow.h"
#include "nsQtMenu.h"
#include "resources.h"
#include "nscore.h"

#include "stdio.h"

nsMenuEventHandler::nsMenuEventHandler(nsBrowserWindow * window)
{
    mWindow = window;
}

void nsMenuEventHandler::MenuItemActivated(int id)
{
    mWindow->DispatchMenuItem(id);
}

void CreateViewerMenus(QWidget *aParent, void * data) 
{
    debug("CreateViewerMenus under (%p)", aParent);
    
    nsBrowserWindow * window = (nsBrowserWindow *) data;

    nsMenuEventHandler * eventHandler = new nsMenuEventHandler(window);

    QMenuBar * menuBar = new QMenuBar(aParent);
    QPopupMenu * file = new QPopupMenu(aParent);
    QObject::connect(file, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * samples = new QPopupMenu(aParent);
    QObject::connect(samples, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * xptests = new QPopupMenu(aParent);
    QObject::connect(xptests, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * edit = new QPopupMenu(aParent);
    QObject::connect(edit, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * debug = new QPopupMenu(aParent);
    QObject::connect(debug, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * style = new QPopupMenu(aParent);
    QObject::connect(style, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * tools = new QPopupMenu(aParent);
    QObject::connect(tools, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * select = new QPopupMenu(aParent);
    QObject::connect(select, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * compatibility = new QPopupMenu(aParent);
    QObject::connect(compatibility, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));
    QPopupMenu * render = new QPopupMenu(aParent);
    QObject::connect(render, 
                     SIGNAL(activated(int)), 
                     eventHandler, 
                     SLOT(MenuItemActivated(int)));

    InsertMenuItem(file, "New Window", eventHandler, VIEWER_WINDOW_OPEN);
    InsertMenuItem(file, "Open", eventHandler, VIEWER_FILE_OPEN);
    InsertMenuItem(file, "View Source", eventHandler, VIEW_SOURCE);

    InsertMenuItem(samples, "demo #0", eventHandler, VIEWER_DEMO0);
    InsertMenuItem(samples, "demo #1", eventHandler, VIEWER_DEMO1);
    InsertMenuItem(samples, "demo #2", eventHandler, VIEWER_DEMO2);
    InsertMenuItem(samples, "demo #3", eventHandler, VIEWER_DEMO3);
    InsertMenuItem(samples, "demo #4", eventHandler, VIEWER_DEMO4);
    InsertMenuItem(samples, "demo #5", eventHandler, VIEWER_DEMO5);
    InsertMenuItem(samples, "demo #6", eventHandler, VIEWER_DEMO6);
    InsertMenuItem(samples, "demo #7", eventHandler, VIEWER_DEMO7);
    InsertMenuItem(samples, "demo #8", eventHandler, VIEWER_DEMO8);
    InsertMenuItem(samples, "demo #9", eventHandler, VIEWER_DEMO9);
    InsertMenuItem(samples, "demo #10", eventHandler, VIEWER_DEMO10);
    InsertMenuItem(samples, "demo #11", eventHandler, VIEWER_DEMO11);
    InsertMenuItem(samples, "demo #12", eventHandler, VIEWER_DEMO12);
    InsertMenuItem(samples, "demo #13", eventHandler, VIEWER_DEMO13);
    InsertMenuItem(samples, "demo #14", eventHandler, VIEWER_DEMO14);
    InsertMenuItem(samples, "demo #15", eventHandler, VIEWER_DEMO15);

    file->insertItem("Samples", samples);

    InsertMenuItem(file, "Test Sites", eventHandler, VIEWER_TOP100);

    InsertMenuItem(xptests, "Toolbar Test 1", eventHandler, VIEWER_XPTOOLKITTOOLBAR1);
    InsertMenuItem(xptests, "Tree Test 1", eventHandler, VIEWER_XPTOOLKITTREE1);

    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(file, "Print Preview", eventHandler, VIEWER_ONE_COLUMN);
    InsertMenuItem(file, "Print", eventHandler, VIEWER_PRINT);
    InsertMenuItem(file, "Print Setup", eventHandler, VIEWER_PRINT_SETUP);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(file, "Exit", eventHandler, VIEWER_EXIT);

    InsertMenuItem(edit, "Cut", eventHandler, VIEWER_EDIT_CUT);
    InsertMenuItem(edit, "Copy", eventHandler, VIEWER_EDIT_COPY);
    InsertMenuItem(edit, "Paste", eventHandler, VIEWER_EDIT_PASTE);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(edit, "Select All", eventHandler, VIEWER_EDIT_SELECTALL);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(edit, "Find in Page", eventHandler, VIEWER_EDIT_FINDINPAGE);

    InsertMenuItem(debug, "Visual Debugging", eventHandler, VIEWER_VISUAL_DEBUGGING);
    InsertMenuItem(debug, "Reflow Test", eventHandler, VIEWER_REFLOW_TEST);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Dump Content", eventHandler, VIEWER_DUMP_CONTENT);
    InsertMenuItem(debug, "Dump Frames", eventHandler, VIEWER_DUMP_FRAMES);
    InsertMenuItem(debug, "Dump Views", eventHandler, VIEWER_DUMP_VIEWS);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Dump Style Sheets", eventHandler, VIEWER_DUMP_STYLE_SHEETS);
    InsertMenuItem(debug, "Dump Style Contexts", eventHandler, VIEWER_DUMP_STYLE_CONTEXTS);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Show Content Size", eventHandler, VIEWER_SHOW_CONTENT_SIZE);
    InsertMenuItem(debug, "Show Frame Size", eventHandler, VIEWER_SHOW_FRAME_SIZE);
    InsertMenuItem(debug, "Show Style Size", eventHandler, VIEWER_SHOW_STYLE_SIZE);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Debug Save", eventHandler, VIEWER_DEBUGSAVE);
    InsertMenuItem(debug, "Debug Output Text", eventHandler, VIEWER_DISPLAYTEXT);
    InsertMenuItem(debug, "Debug Output HTML", eventHandler, VIEWER_DISPLAYHTML);
    InsertMenuItem(debug, "Debug Toggle Selection", eventHandler, VIEWER_TOGGLE_SELECTION);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Debug Robot", eventHandler, VIEWER_DEBUGROBOT);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Show Content Quality", eventHandler, VIEWER_SHOW_CONTENT_QUALITY);
    InsertMenuItem(file, nsnull, nsnull, 0);
    debug->insertItem("Style", style);

    InsertMenuItem(select, "List Available Sheets", eventHandler, VIEWER_SELECT_STYLE_LIST);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(select, "Select Default", eventHandler, VIEWER_SELECT_STYLE_DEFAULT);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(select, "Select Alternative 1", eventHandler, VIEWER_SELECT_STYLE_ONE);
    InsertMenuItem(select, "Select Alternative 2", eventHandler, VIEWER_SELECT_STYLE_TWO);
    InsertMenuItem(select, "Select Alternative 3", eventHandler, VIEWER_SELECT_STYLE_THREE);
    InsertMenuItem(select, "Select Alternative 4", eventHandler, VIEWER_SELECT_STYLE_FOUR);

    style->insertItem("Select Style Sheet", select);

    InsertMenuItem(compatibility, "Nav Quirks", eventHandler, VIEWER_NAV_QUIRKS_MODE);
    InsertMenuItem(compatibility, "Standard", eventHandler, VIEWER_STANDARD_MODE);

    style->insertItem("Compatibility Mode", compatibility);

    InsertMenuItem(render, "Native", eventHandler, VIEWER_NATIVE_WIDGET_MODE);
    InsertMenuItem(render, "Gfx", eventHandler, VIEWER_GFX_WIDGET_MODE);

    style->insertItem("Widget Render Mode", render);

    InsertMenuItem(tools, "JavaScript Console", eventHandler, JS_CONSOLE);
    InsertMenuItem(tools, "Editor Mode", eventHandler, EDITOR_MODE);

    menuBar->insertItem("&File", file);
    menuBar->insertItem("&Edit", edit);
    menuBar->insertItem("&Debug", debug);
    menuBar->insertItem("&Tools", tools);

    menuBar->show();
}


void InsertMenuItem(QPopupMenu * popup, const char * string, QObject * receiver, int id)
{
    if (popup)
    {
        if (string)
        {
            popup->insertItem(string, id);

            //popup->connectItem(id, receiver, SLOT(MenuItemActivated(int))); 
#if 0
            popup->insertItem(string, 
                              receiver, 
                              SLOT(MenuItemActivated(int)), 
                              0, 
                              id);
#endif
        }
        else
        {
            popup->insertSeparator();
        }
    }
}
