/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMotifMenu_h_
#define _nsMotifMenu_h_

#include "nscore.h"

#include <qpopupmenu.h>
#include <qmenubar.h>

class nsBrowserWindow;

class nsMenuEventHandler : public QObject
{
    Q_OBJECT
public:
    nsMenuEventHandler(nsBrowserWindow * window);

public slots:
    void MenuItemActivated(int id);

private:
    nsBrowserWindow * mWindow;
};

void CreateViewerMenus(QWidget *aParent, 
                       void * data, 
                       PRInt32 * aMenuBarHeight);
void InsertMenuItem(QPopupMenu * popup, 
                    const char * string, 
                    QObject * receiver, 
                    int id);

#endif // _nsMotifMenu_h_
