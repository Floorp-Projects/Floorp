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
#include "nsCheckButton.h"
#include "nsFileWidget.h"
#include "nsGUIEvent.h"

#include "stdio.h"

#define DBG 0

nsRect gResizeRect;
int gResized = 0;

//==============================================================
void nsXtWidget_InitNSEvent(XEvent   * anXEv,
                            XtPointer  p,
                            nsGUIEvent &anEvent,
                            PRUint32   aEventType) 
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWindow *) p;

  anEvent.point.x = anXEv->xbutton.x;
  anEvent.point.y = anXEv->xbutton.y;

  anEvent.time    = 0; //TBD

}

//==============================================================
void nsXtWidget_InitNSMouseEvent(XEvent   * anXEv,
                                 XtPointer  p,
                                 nsMouseEvent &anEvent,
                                 PRUint32   aEventType) 
{
  // Do base initialization
  nsXtWidget_InitNSEvent(anXEv, p, anEvent, aEventType);

  // Do Mouse Event specific intialization
  anEvent.time      = anXEv->xbutton.time;
  anEvent.isShift   = anXEv->xbutton.state | ShiftMask;
  anEvent.isControl = anXEv->xbutton.state | ControlMask;

  //anEvent.isAlt      = GetKeyState(VK_LMENU) < 0    || GetKeyState(VK_RMENU) < 0;
  ////anEvent.clickCount = (aEventType == NS_MOUSE_LEFT_DOUBLECLICK ||
                      //aEventType == NS_MOUSE_LEFT_DOUBLECLICK)? 2:1;

}

//==============================================================
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define INTERSECTS(r1_x1,r1_x2,r1_y1,r1_y2,r2_x1,r2_x2,r2_y1,r2_y2) \
        !((r2_x2 <= r1_x1) ||\
          (r2_y2 <= r1_y1) ||\
          (r2_x1 >= r1_x2) ||\
          (r2_y1 >= r1_y2))

//==============================================================
typedef struct COLLAPSE_INFO {
    Window win;
    nsRect *r;
} CollapseInfo;

//==============================================================
static void expandDamageRect(nsRect *drect, XEvent *xev, Boolean debug, char*str)
{
    int x1 = xev->xexpose.x;
    int y1 = xev->xexpose.y;
    int x2 = x1 + xev->xexpose.width;
    int y2 = y1 + xev->xexpose.height;

    if (debug) {
        printf("   %s: collapsing (%d,%d %dx%d) into (%d,%d %dx%d) ->>",
               str, x1, y1, xev->xexpose.width, xev->xexpose.height,
               drect->x, drect->y, drect->width - drect->x, drect->height - drect->y); 
    } 
    
    drect->x = MIN(x1, drect->x);
    drect->y = MIN(y1, drect->y);
    drect->width = MAX(x2, drect->width);
    drect->height = MAX(y2, drect->height);
    
    if (debug) {
        printf("(%d,%d %dx%d) %s\n",
               drect->x, drect->y, drect->width - drect->x, drect->height - drect->y);
    }
}

//==============================================================
static Bool checkForExpose(Display *dpy, XEvent *evt, XtPointer client_data) 
{
    CollapseInfo *cinfo = (CollapseInfo*)client_data; 

    if ((evt->type == Expose && evt->xexpose.window == cinfo->win &&
         INTERSECTS(cinfo->r->x, cinfo->r->width, cinfo->r->y, cinfo->r->height,
                    evt->xexpose.x, evt->xexpose.y, 
                    evt->xexpose.x + evt->xexpose.width, 
                    evt->xexpose.y + evt->xexpose.height)) ||
  (evt->type == GraphicsExpose && evt->xgraphicsexpose.drawable == cinfo->win &&
         INTERSECTS(cinfo->r->x, cinfo->r->width, cinfo->r->y, cinfo->r->height,
                    evt->xgraphicsexpose.x, evt->xgraphicsexpose.y, 
                    evt->xgraphicsexpose.x + evt->xgraphicsexpose.width, 
                    evt->xgraphicsexpose.y + evt->xgraphicsexpose.height))) {

        return True;
    }
    return False;
}


//==============================================================
void nsXtWidget_ExposureMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{

  if (DBG) fprintf(stderr, "In nsXtWidget_ExposureMask_EventHandler Type %d (%d,%d)\n", event->type, Expose, GraphicsExpose);
  nsWindow * widgetWindow = (nsWindow *) p ;

  nsPaintEvent pevent;
  nsRect       rect;
  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);
  pevent.rect = (nsRect *)&rect;
  XEvent xev;

#if 0
  int count = 0;
  while (XPeekEvent(XtDisplay(w), &xev))
  {
     if ((xev.type == Expose || xev.type == GraphicsExpose || xev.type == 14) && 
         (xev.xexpose.window == XtWindow(w))) {
       XNextEvent(XtDisplay(w), &xev);
       count++;
     } else {
       if (DBG) printf("Ate %d events\n", count);
       break;
     }
  }
#endif

  widgetWindow->OnPaint(pevent);

#if 0

  nsPaintEvent pevent;
  nsRect       rect;
  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT); 

  pevent.rect = (nsRect *)&rect;
printf("Count %d\n", event->xexpose.count);
  if (event->xexpose.count != 0)
    return ;

  /* Only post Expose/Repaint if we know others arn't following
   * directly in the queue.
   */
  if (event->xexpose.count == 0) {
    Boolean      debug = PR_TRUE;
    int          count = 0;
    CollapseInfo cinfo;

    cinfo.win = XtWindow(w);
    cinfo.r   = pevent.rect;

    rect.x      = event->xexpose.x;
    rect.y      = event->xexpose.y;
    rect.width  = event->xexpose.width;
    rect.height = event->xexpose.height;
printf("Before %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
    /* Do a little more inspecting and collapse further if there
     * are additional expose events pending on this window where
     * the damage rects intersect with the current exposeRect.
     */
    while (1) {
      XEvent xev;

      if (XCheckIfEvent(XtDisplay(w), &xev, checkForExpose, (XtPointer)&cinfo)) {
        printf("]]]]]]]]]]]]]]]]]]]]]]]]]]]]]\n");
        count = xev.xexpose.count;
        expandDamageRect(&rect, &xev, debug, "2");
        
      } else /* XCheckIfEvent Failed. */
       break;
    }
printf("After %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
  }

  widgetWindow->OnPaint(pevent);

  if (DBG) fprintf(stderr, "Out nsXtWidget_ExposureMask_EventHandler\n");
#endif
}

//==============================================================
void nsXtWidget_ButtonPressMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_ButtonReleaseMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_ButtonMotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  //if (DBG) fprintf(stderr, "nsXtWidget_ButtonMotionMask_EventHandler\n");
  nsPaintEvent pevent ;
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

}

//==============================================================
void nsXtWidget_MotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  //if (DBG) fprintf(stderr, "nsXtWidget_ButtonMotionMask_EventHandler\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

}

//==============================================================
void nsXtWidget_EnterMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_ENTER);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_LeaveMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  if (DBG) fprintf(stderr, "***************** nsXtWidget_LeaveMask_EventHandler\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_EXIT);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_Toggle_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsXtWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  /*nsGUIEvent event;
  nsXtWidget_InitNSEvent(event, p, event, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);
  */
}

//==============================================================
void nsXtWidget_Toggle_ArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  nsCheckButton * checkBtn = (nsCheckButton *) p ;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);
  checkBtn->Armed();

}

//==============================================================
void nsXtWidget_Toggle_DisArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  if (DBG) fprintf(stderr, "In ***************** nsXtWidget_Scrollbar_Callback\n");
  nsCheckButton * checkBtn = (nsCheckButton *) p ;

  nsScrollbarEvent sevent;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  checkBtn->DisArmed();
  if (DBG) fprintf(stderr, "Out ***************** nsXtWidget_Scrollbar_Callback\n");
  
}

//==============================================================
void nsXtWidget_Scrollbar_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsXtWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;

  XmScrollBarCallbackStruct * cbs = (XmScrollBarCallbackStruct*) call_data;
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  sevent.widget  = (nsWindow *) p;
  if (cbs->event != nsnull) {
    sevent.point.x = cbs->event->xbutton.x;
    sevent.point.y = cbs->event->xbutton.y;
  } else {
    sevent.point.x = 0;
    sevent.point.y = 0;
  }
  sevent.time    = 0; //TBD

  switch (cbs->reason) {

    case XmCR_INCREMENT:
      sevent.message = NS_SCROLLBAR_LINE_NEXT;
      break;

    case XmCR_DECREMENT:
      sevent.message = NS_SCROLLBAR_LINE_PREV;
      break;

    case XmCR_PAGE_INCREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_NEXT;
      break;

    case XmCR_PAGE_DECREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_PREV;
      break;

    case XmCR_DRAG:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    case XmCR_VALUE_CHANGED:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    default:
      if (DBG) fprintf(stderr, "In Default processing for scrollbar reason is [%d]\n", cbs->reason);
      break;
  }
  widgetWindow->OnScroll(sevent, cbs->value);
}



//==============================================================
void nsXtWidget_Expose_Callback(Widget w, XtPointer p, XtPointer call_data)
{

  //if (DBG) 
//fprintf(stderr, "In nsXtWidget_Resize_Callback 0x%x", p);
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (widgetWindow == nsnull) {
    return;
  }

  XmDrawingAreaCallbackStruct * cbs = (XmDrawingAreaCallbackStruct *)call_data;
  XEvent * event = cbs->event;
  nsPaintEvent pevent;
  nsRect       rect;
  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);

  pevent.rect = (nsRect *)&rect;
#if 0
printf("Count %d\n", event->xexpose.count);
  if (event->xexpose.count != 0)
    return ;

  /* Only post Expose/Repaint if we know others arn't following
   * directly in the queue.
   */
  if (event->xexpose.count == 0) {
    Boolean      debug = PR_TRUE;
    int          count = 0;
    CollapseInfo cinfo;

    cinfo.win = XtWindow(w);
    cinfo.r   = pevent.rect;

    rect.x      = event->xexpose.x;
    rect.y      = event->xexpose.y;
    rect.width  = event->xexpose.width;
    rect.height = event->xexpose.height;
printf("Before %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
    /* Do a little more inspecting and collapse further if there
     * are additional expose events pending on this window where
     * the damage rects intersect with the current exposeRect.
     */
    while (1) {
      XEvent xev;

      if (XCheckIfEvent(XtDisplay(w), &xev, checkForExpose, (XtPointer)&cinfo)) {
        printf("]]]]]]]]]]]]]]]]]]]]]]]]]]]]]\n");
        count = xev.xexpose.count;
        expandDamageRect(&rect, &xev, debug, "2");

      } else /* XCheckIfEvent Failed. */
       break;
    }
printf("After %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
  }
#endif
  widgetWindow->OnPaint(pevent);

  if (DBG) fprintf(stderr, "Out nsXtWidget_ExposureMask_EventHandler\n");


}

//==============================================================
void nsXtWidget_Resize_Callback(Widget w, XtPointer p, XtPointer call_data)
{

  //if (DBG) 
//fprintf(stderr, "In nsXtWidget_Resize_Callback 0x%x", p);
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (widgetWindow == nsnull) {
    return;
  }

  XmDrawingAreaCallbackStruct * cbs = (XmDrawingAreaCallbackStruct *)call_data;

  //fprintf(stderr, "  %s  ** %s\n", widgetWindow->gInstanceClassName, 
     //cbs->reason == XmCR_RESIZE?"XmCR_RESIZE":"XmCR_EXPOSE");

  /*XEvent * xev = cbs->event;
  if (xev != nsnull) {
    //printf("Width %d   Height %d\n", xev->xresizerequest.width, 
       //xev->xresizerequest.height);
  } else {
    //printf("Jumping out ##################################\n");
    //return;
  }*/

  /*if (cbs->reason == XmCR_EXPOSE && widgetWindow->IgnoreResize()) {
    cbs->reason = XmCR_RESIZE;
    widgetWindow->SetIgnoreResize(PR_FALSE);
    printf("Got Expose doing resize!\n");
  } else if (widgetWindow->IgnoreResize() || 
             (!widgetWindow->IgnoreResize() && cbs->reason == XmCR_RESIZE)) {
    printf("Skipping resize!\n");
    widgetWindow->SetIgnoreResize(PR_TRUE);
    return;
  }*/

  if (cbs->reason == XmCR_RESIZE) {
    nsSizeEvent event;
    nsRect rect;
    event.message = NS_SIZE;
    event.widget  = (nsWindow *) p;
    if (cbs->event != NULL) {
      event.point.x = cbs->event->xbutton.x;
      event.point.y = cbs->event->xbutton.y;
    }
    event.time    = 0; //TBD
    event.windowSize = (nsRect *)&rect;
    widgetWindow->GetBounds(rect);

    Window win = nsnull;
    if (widgetWindow) {
      win = XtWindow(w);
    }

    if (widgetWindow && win) {
      XWindowAttributes attrs ;

      Display * d = XtDisplay(w);

      XGetWindowAttributes(d, win, &attrs);

      PRBool doResize = PR_FALSE;
      if (attrs.width > 0 && 
        rect.width != attrs.width) {   
        rect.width = attrs.width;
        doResize = PR_TRUE;
      }
      if (attrs.height > 0 &&
          rect.height != attrs.height) {  
        rect.height = attrs.height;
        doResize = PR_TRUE;
      }
      //printf("doResize %s  %d %d %d %d rect %d %d\n",  (doResize ?"true":"false"),
             //attrs.x, attrs.y, attrs.width, attrs.height, rect.width, rect.height);

        // NOTE: THIS May not be needed when embedded in chrome
      printf("Adding timeout\n");

extern XtAppContext gAppContext;

      if (! gResized)
        XtAppAddTimeOut(gAppContext, 1000, (XtTimerCallbackProc)nsXtWidget_Refresh_Callback, widgetWindow);

      gResizeRect.x = rect.x;
      gResizeRect.y = rect.y;
      gResizeRect.width = rect.width;
      gResizeRect.height = rect.height;
      gResized = 1;

#if 0
      //doResize = PR_TRUE;
      if (doResize) {
        //printf("??????????????????????????????? Doing Resize\n");
        widgetWindow->SetBounds(rect); // This needs to be done inside OnResize
        widgetWindow->OnResize(event);
      }
#endif
    }
  }
}

//==============================================================
void nsXtWidget_Text_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  if (DBG) fprintf(stderr, "In nsXtWidget_Text_Callback\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  static char * passwd;
  char * newStr;
  int len;

  XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *) call_data;

  if (cbs->reason == XmCR_ACTIVATE) {
      //printf ("Password: %s\n", passwd);
      return;
  }

  if (cbs->startPos < cbs->currInsert) {   /* backspace */
      cbs->endPos = strlen (passwd);       /* delete from here to end */
      passwd[cbs->startPos] = 0;           /* backspace--terminate */
      return;
  }

  if (cbs->text->length > 1) {
      cbs->doit = False;  /* don't allow "paste" operations */
      return;             /* make the user *type* the password! */
  }

  newStr = XtMalloc (cbs->endPos + 2); /* new char + NULL terminator */
  if (passwd) {
      strcpy (newStr, passwd);
      XtFree (passwd);
  } else
      newStr[0] = 0;
  passwd = newStr;
  strncat (passwd, cbs->text->ptr, cbs->text->length);
  passwd[cbs->endPos + cbs->text->length] = 0;

  for (len = 0; len < cbs->text->length; len++)
      cbs->text->ptr[len] = '*';
  if (DBG) fprintf(stderr, "Out nsXtWidget_Text_Callback\n");
}

//==============================================================
void nsXtWidget_FSBCancel_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsFileWidget * widgetWindow = (nsFileWidget *) p ;
  if (p != nsnull) {
    printf("OnCancel\n");
    widgetWindow->OnCancel();
  }
}

//==============================================================
void nsXtWidget_FSBOk_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsFileWidget * widgetWindow = (nsFileWidget *) p ;
  if (p != nsnull) {
    widgetWindow->OnOk();
  }
}

void nsXtWidget_Refresh_Callback(XtPointer call_data)
{
    nsSizeEvent event;
    event.message = NS_SIZE;
    event.widget  = (nsWindow *) call_data;
    event.time    = 0; //TBD
    event.windowSize = (nsRect *)&gResizeRect;

 printf("In timer\n");
 nsWindow* widgetWindow = (nsWindow*)call_data;
 widgetWindow->SetBounds(gResizeRect); // This needs to be done inside OnResize
 widgetWindow->OnResize(event);
 gResized = 0;
}
