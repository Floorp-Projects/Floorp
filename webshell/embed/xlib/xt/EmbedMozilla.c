/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 * 
 * Contributor(s): 
 */

#include <stdio.h>
#include <stdlib.h>

#include "EmbedMozilla.h"
#include "EmbedMozillaP.h"
#include "nslog.h"

NS_IMPL_LOG(EmbedMozillaLog, 0)
#define PRINTF NS_LOG_PRINTF(EmbedMozillaLog)
#define FLUSH  NS_LOG_FLUSH(EmbedMozillaLog)

static XtGeometryResult
QueryGeometry( Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply );

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply);

static void ChangeManaged(Widget w);

static void	CoreRealize			(Widget,XtValueMask *,XSetWindowAttributes *);

static void Resize(Widget w);


XtEmbedMozillaClassRec xtEmbedMozillaClassRec =
{
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &constraintClassRec,
    /* class_name         */    "XtEmbedMozilla",
    /* widget_size        */    sizeof(XtEmbedMozillaRec),
    /* class_initialize   */    NULL,
    /* class_partinit     */    NULL /* ClassPartInitialize */,
    /* class_inited       */	  FALSE,
    /* initialize         */    NULL /* Initialize */,
    /* Init hook	        */    NULL,
    /* realize            */    CoreRealize,
    /* actions		        */	  NULL,
    /* num_actions	      */	  0,
    /* resources          */    NULL /* resources */,
    /* num_resources      */    0 /* XtNumber(resources) */,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	  TRUE,
    /* compress_exposure  */	  TRUE,
    /* compress_enterleave*/	  TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    Resize,
    /* expose             */    XtInheritExpose /* (XtExposeProc) Redisplay */,
    /* set_values         */    NULL /* (XtSetValuesFunc )SetValues */,
    /* set values hook    */    NULL,
    /* set values almost  */    XtInheritSetValuesAlmost,
    /* get values hook    */    NULL,
    /* accept_focus       */    NULL,
    /* Version            */    XtVersion,
    /* PRIVATE cb list    */    NULL,
    /* tm_table		        */    XtInheritTranslations,
    /* query_geometry     */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension          */    NULL,
  },
  {
/* composite_class fields */
    /* geometry_manager   */    XtInheritGeometryManager, //GeometryManager,
    /* change_managed     */    XtInheritChangeManaged, //ChangeManaged,
    /* insert_child	      */    XtInheritInsertChild,
    /* delete_child	      */    XtInheritDeleteChild,
    /* Extension          */    NULL,
  },
	{
/* Constraint class Init */
    NULL,
    0,
    0,
    NULL,
    NULL,
    NULL,
    NULL
      
  },
	{
/* XtEmbedMozilla class - none */     
     /* mumble */               0
	}
};


WidgetClass xtEmbedMozillaClass = (WidgetClass)&xtEmbedMozillaClassRec;

#define XtX(w)            w->core.x
#define XtY(w)            w->core.y
#define XtWidth(w)        w->core.width
#define XtHeight(w)       w->core.height
#define XtBorderWidth(w)  w->core.border_width

static XtGeometryResult
QueryGeometry( Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply )
{
	if ( request->request_mode == 0 )
		return XtGeometryYes;

	reply->request_mode &= CWWidth | CWHeight;
	
	if ( request->request_mode & CWWidth )
		reply->width = request->width;

	if ( request->request_mode & CWHeight )
		reply->height = request->height;

	return XtGeometryYes;
}

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
	if (request->request_mode & XtCWQueryOnly)
	{
		return(XtGeometryYes);
	}

	if (request->request_mode & CWX)
	{
		XtX(w) = reply->x = request->x;
	}
	if (request->request_mode & CWY)
	{
		XtY(w) = reply->y = request->y;
	}
	if (request->request_mode & CWWidth)
	{
		XtWidth(w) = reply->width = request->width;
	}
	if (request->request_mode & CWHeight)
	{
		XtHeight(w) = reply->height = request->height;
	}
	if (request->request_mode & CWBorderWidth)
	{
		XtBorderWidth(w) = reply->border_width = request->border_width;
	}

	return(XtGeometryYes);
}


static void
ChangeManaged(Widget w)
{
	return;
}

static void Resize (Widget widget)
{
  XtEmbedMozilla em = (XtEmbedMozilla) widget;

#if 0
  /* Invoke the resize procedure of the superclass.
     Probably there's some nominally more portable way to do this
     (yeah right, like any of these slot names could possibly change
     and have any existing code still work.)
   */
  widget->core.widget_class->core_class.superclass->core_class.resize (widget);

  /* Now run our callback (yeah, I should use a real callback, so sue me.) */
  scroller->scroller.resize_hook (widget, scroller->scroller.resize_arg);
#endif

}

static void
CoreRealize(Widget w,XtValueMask *mask,XSetWindowAttributes* wa)
{
  XtEmbedMozilla em = (XtEmbedMozilla) w;

  PRINTF("CoreRealize(%s),window = %ld\n", 
         XtName(w), em->embed_mozilla.embed_window);
  PRINTF("CoreRealize: parent widget = %p\n", em->core.parent );
  
  em->core.window = em->embed_mozilla.embed_window;
}
/*----------------------------------------------------------------------*/

Widget
XtCreateEmbedMozilla(Widget	  parent,
                     Window   window,
                     String	  name,
                     Arg      *av,
                     Cardinal	ac)
{
  Widget w = XtCreateWidget(name,xtEmbedMozillaClass,parent,av,ac);

  XtEmbedMozilla em = (XtEmbedMozilla) w;

  em->embed_mozilla.embed_window = window;

  return w;
}
/*----------------------------------------------------------------------*/
