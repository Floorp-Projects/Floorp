/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *     Lars Knoll <knoll@kde.org>
 *     Zack Rusin <zack@kde.org>
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

#include <qwidget.h>
#include "nsBrowserWindow.h"
#include "nsQtMenu.h"
#include "resources.h"
#include "nscore.h"
#include <stdio.h>

nsMenuEventHandler::nsMenuEventHandler(nsBrowserWindow * window)
{
    mWindow = window;
}

void nsMenuEventHandler::MenuItemActivated(int id)
{
    mWindow->DispatchMenuItem(id);
}

void CreateViewerMenus(nsIWidget *  aParentInterface, void *data, QWidget ** aMenuBarOut)
{
    QWidget *aParent = (QWidget *)aParentInterface->GetNativeData(NS_NATIVE_WIDGET);

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
    QPopupMenu * eventdebug = new QPopupMenu(aParent);
    QObject::connect(eventdebug,
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

    InsertMenuItem(file, "&New Window", eventHandler, VIEWER_WINDOW_OPEN);
    InsertMenuItem(file, "&Open", eventHandler, VIEWER_FILE_OPEN);
    InsertMenuItem(file, "&View Source", eventHandler, VIEW_SOURCE);

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
    InsertMenuItem(samples, "demo #16", eventHandler, VIEWER_DEMO16);
    InsertMenuItem(samples, "demo #17", eventHandler, VIEWER_DEMO17);

    file->insertItem("&Samples", samples);

    InsertMenuItem(file, "&Test Sites", eventHandler, VIEWER_TOP100);

    InsertMenuItem(xptests, "Toolbar Test 1", eventHandler, VIEWER_XPTOOLKITTOOLBAR1);
    InsertMenuItem(xptests, "Tree Test 1", eventHandler, VIEWER_XPTOOLKITTREE1);

    file->insertItem("XPToolkit Tests", xptests);

    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(file, "Print Preview", eventHandler, VIEWER_ONE_COLUMN);
    InsertMenuItem(file, "Print", eventHandler, VIEWER_PRINT);
    InsertMenuItem(file, nsnull, nsnull, 0);
    InsertMenuItem(file, "&Exit", eventHandler, VIEWER_EXIT);

    InsertMenuItem(edit, "Cu&t", eventHandler, VIEWER_EDIT_CUT);
    InsertMenuItem(edit, "&Copy", eventHandler, VIEWER_EDIT_COPY);
    InsertMenuItem(edit, "&Paste", eventHandler, VIEWER_EDIT_PASTE);
    InsertMenuItem(edit, nsnull, nsnull, 0);
    InsertMenuItem(edit, "Select All", eventHandler, VIEWER_EDIT_SELECTALL);
    InsertMenuItem(edit, nsnull, nsnull, 0);
    InsertMenuItem(edit, "Find in Page", eventHandler, VIEWER_EDIT_FINDINPAGE);

    InsertMenuItem(debug, "&Visual Debugging", eventHandler, VIEWER_VISUAL_DEBUGGING);
    InsertMenuItem(debug, nsnull, nsnull, 0);

    InsertMenuItem(eventdebug, "Toggle Paint Flashing", eventHandler, VIEWER_TOGGLE_PAINT_FLASHING);
    InsertMenuItem(eventdebug, "Toggle Paint Dumping", eventHandler, VIEWER_TOGGLE_PAINT_DUMPING);
    InsertMenuItem(eventdebug, "Toggle Invalidate Dumping", eventHandler, VIEWER_TOGGLE_INVALIDATE_DUMPING);
    InsertMenuItem(eventdebug, "Toggle Event Dumping", eventHandler, VIEWER_TOGGLE_EVENT_DUMPING);
    InsertMenuItem(eventdebug, nsnull, nsnull, 0);
    InsertMenuItem(eventdebug, "Toggle Motion Event Dumping", eventHandler, VIEWER_TOGGLE_MOTION_EVENT_DUMPING);
    InsertMenuItem(eventdebug, "Toggle Crossing Event Dumping", eventHandler, VIEWER_TOGGLE_CROSSING_EVENT_DUMPING);

    debug->insertItem("Event Debugging", eventdebug);

    InsertMenuItem(debug, nsnull, nsnull, 0);
    InsertMenuItem(debug, "&Reflow Test", eventHandler, VIEWER_REFLOW_TEST);
    InsertMenuItem(debug, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Dump &Content", eventHandler, VIEWER_DUMP_CONTENT);
    InsertMenuItem(debug, "Dump &Frames", eventHandler, VIEWER_DUMP_FRAMES);
    InsertMenuItem(debug, "Dump &Views", eventHandler, VIEWER_DUMP_VIEWS);
    InsertMenuItem(debug, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Dump &Style Sheets", eventHandler, VIEWER_DUMP_STYLE_SHEETS);
    InsertMenuItem(debug, "Dump &Style Contexts", eventHandler, VIEWER_DUMP_STYLE_CONTEXTS);
    InsertMenuItem(debug, nsnull, nsnull, 0);
//     InsertMenuItem(debug, "Show Content Size", eventHandler, VIEWER_SHOW_CONTENT_SIZE);
//     InsertMenuItem(debug, "Show Frame Size", eventHandler, VIEWER_SHOW_FRAME_SIZE);
//     InsertMenuItem(debug, "Show Style Size", eventHandler, VIEWER_SHOW_STYLE_SIZE);
    InsertMenuItem(debug, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Debug Save", eventHandler, VIEWER_DEBUGSAVE);
    InsertMenuItem(debug, "Debug Output Text", eventHandler, VIEWER_DISPLAYTEXT);
    InsertMenuItem(debug, "Debug Output HTML", eventHandler, VIEWER_DISPLAYHTML);
    InsertMenuItem(debug, "Debug Toggle Selection", eventHandler, VIEWER_TOGGLE_SELECTION);
    InsertMenuItem(debug, nsnull, nsnull, 0);
    InsertMenuItem(debug, "Debug Robot", eventHandler, VIEWER_DEBUGROBOT);
    InsertMenuItem(debug, nsnull, nsnull, 0);
//     InsertMenuItem(debug, "Show Content Quality", eventHandler, VIEWER_SHOW_CONTENT_QUALITY);
    InsertMenuItem(debug, nsnull, nsnull, 0);
    debug->insertItem("Style", style);

    InsertMenuItem(select, "List Available Sheets", eventHandler, VIEWER_SELECT_STYLE_LIST);
    InsertMenuItem(select, nsnull, nsnull, 0);
    InsertMenuItem(select, "Select Default", eventHandler, VIEWER_SELECT_STYLE_DEFAULT);
    InsertMenuItem(select, nsnull, nsnull, 0);
    InsertMenuItem(select, "Select Alternative 1", eventHandler, VIEWER_SELECT_STYLE_ONE);
    InsertMenuItem(select, "Select Alternative 2", eventHandler, VIEWER_SELECT_STYLE_TWO);
    InsertMenuItem(select, "Select Alternative 3", eventHandler, VIEWER_SELECT_STYLE_THREE);
    InsertMenuItem(select, "Select Alternative 4", eventHandler, VIEWER_SELECT_STYLE_FOUR);

    style->insertItem("Select &Style Sheet", select);

    InsertMenuItem(compatibility, "Use DTD", eventHandler, VIEWER_USE_DTD_MODE);
    InsertMenuItem(compatibility, "Nav Quirks", eventHandler, VIEWER_NAV_QUIRKS_MODE);
    InsertMenuItem(compatibility, "Standard", eventHandler, VIEWER_STANDARD_MODE);

    style->insertItem("&Compatibility Mode", compatibility);

    InsertMenuItem(render, "Native", eventHandler, VIEWER_NATIVE_WIDGET_MODE);
    InsertMenuItem(render, "Gfx", eventHandler, VIEWER_GFX_WIDGET_MODE);

    style->insertItem("&Widget Render Mode", render);

    InsertMenuItem(tools, "&JavaScript Console", eventHandler, JS_CONSOLE);
    InsertMenuItem(tools, "&Editor Mode", eventHandler, EDITOR_MODE);

    menuBar->insertItem("&File", file);
    menuBar->insertItem("&Edit", edit);
    menuBar->insertItem("&Debug", debug);
    menuBar->insertItem("&Tools", tools);

    qDebug("menubar height = %d", menuBar->height());
    menuBar->resize(menuBar->sizeHint());
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
