/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
