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

#include "Xm/Xm.h"
#include "nsXtEventHandler.h"

#include "nsWindow.h"

#include "stdio.h"

void nsXtWidget_ExposureMask_EventHandler(Widget w, caddr_t client_data, XEvent * event)
{
  nsPaintEvent pevent ;
  nsWindow * widgetWindow = (nsWindow *) client_data ;

  if (event->xexpose.count != 0)
    return ;

  pevent.widget = widgetWindow;
    
  pevent.point.x = event->xbutton.x;
  pevent.point.y = event->xbutton.y;

  pevent.time = 0; // XXX TBD...
  pevent.message = NS_PAINT ;  

  widgetWindow->OnPaint(pevent);

}

void nsXtWidget_ButtonPressMask_EventHandler(Widget w, caddr_t client_data, XEvent * event)
{
  nsWindow * widgetWindow = (nsWindow *) client_data ;
}

void nsXtWidget_ButtonReleaseMask_EventHandler(Widget w, caddr_t client_data, XEvent * event)
{
  nsWindow * widgetWindow = (nsWindow *) client_data ;
}

void nsXtWidget_ButtonMotionMask_EventHandler(Widget w, caddr_t client_data, XEvent * event)
{
  nsWindow * widgetWindow = (nsWindow *) client_data ;
}



