/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdio.h>
#include <stdlib.h>

#include "EmbedMozilla.h"
#include "EmbedMozillaP.h"

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply);

static void
ChangeManaged(Widget w);

static void	CoreRealize			(Widget,XtValueMask *,XSetWindowAttributes *);

static void Resize(Widget w);


XmEmbedMozillaClassRec xmEmbedMozillaClassRec =
{
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &xmManagerClassRec,
    /* class_name         */    "XmEmbedMozilla",
    /* widget_size        */    sizeof(XmEmbedMozillaRec),
    /* class_initialize   */    NULL,
    /* class_partinit     */    NULL /* ClassPartInitialize */,
    /* class_inited       */	FALSE,
    /* initialize         */    NULL /* Initialize */,
    /* Init hook	  */    NULL,
                                CoreRealize,
    /* actions		  */	NULL,
    /* num_actions	  */	0,
    /* resources          */    NULL /* resources */,
    /* num_resources      */    0 /* XtNumber(resources) */,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	TRUE,
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
    /* tm_table		  */    XtInheritTranslations,
    /* query_geometry     */    NULL /* QueryProc */,
    /* display_accelerator*/    NULL,
    /* extension          */    NULL,
  },
  {
/* composite_class fields */
    /* geometry_mozillar   */    GeometryManager /*(XtGeometryHandler )GeometryManager*/,
    /* change_mozillad     */    ChangeManaged /*(XtWidgetProc) ChangeManaged*/,
    /* insert_child	  */	XtInheritInsertChild /*(XtArgsProc) InsertChild*/,	
    /* delete_child	  */	XtInheritDeleteChild,
    /* Extension          */    NULL,
  },{
/* Constraint class Init */
    NULL,
    0,
    0,
    NULL,
    NULL,
    NULL,
    NULL
      
  },
/* Mozillar Class */
   {		
      XmInheritTranslations/*ScrolledWindowXlations*/,     /* translations        */    
      NULL /*get_resources*/,			/* get resources      	  */
      0 /*XtNumber(get_resources)*/,		/* num get_resources 	  */
      NULL,					/* get_cont_resources     */
      0,					/* num_get_cont_resources */
      XmInheritParentProcess,                   /* parent_process         */
      NULL,					/* extension           */    
   },

 {
/* XmEmbedMozilla class - none */     
     /* mumble */               0
 }
};


WidgetClass xmEmbedMozillaClass = (WidgetClass)&xmEmbedMozillaClassRec;


static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
	if (request->request_mode & XtCWQueryOnly)
	{
		return(XtGeometryYes);
	}

	if (request->request_mode & CWX)
	{
		XtX(w) = request->x;
	}
	if (request->request_mode & CWY)
	{
		XtY(w) = request->y;
	}
	if (request->request_mode & CWWidth)
	{
		XtWidth(w) = request->width;
	}
	if (request->request_mode & CWHeight)
	{
		XtHeight(w) = request->height;
	}
	if (request->request_mode & CWBorderWidth)
	{
		XtBorderWidth(w) = request->border_width;
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
  XmEmbedMozilla em = (XmEmbedMozilla) widget;

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
  XmEmbedMozilla em = (XmEmbedMozilla) w;

  printf("Realize(%s,window = %p\n",
		 XtName(w),
		 em->embed_mozilla.embed_window);
  
  em->core.window = em->embed_mozilla.embed_window;
}
/*----------------------------------------------------------------------*/

Widget
XmCreateEmbedMozilla(Widget	parent,
                     Window     window,
                     String	name,
                     Arg *	av,
                     Cardinal	ac)
{
  Widget w = XtCreateWidget(name,xmEmbedMozillaClass,parent,av,ac);

  XmEmbedMozilla em = (XmEmbedMozilla) w;

  em->embed_mozilla.embed_window = window;

  return w;
}
/*----------------------------------------------------------------------*/
