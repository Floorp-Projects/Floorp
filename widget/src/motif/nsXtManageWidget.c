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

/* 
   new_manage.c --- defines a subclass of XmManager
   Created: Eric Bina <ebina@netscape.com>, 17-Aug-94.

   Excerpts from the X Toolkit Intrinsics Programming Manual - O'Reilly:

	"Writing a general-purpose composite widget is not a trivial task
	and should only be done when other options fail."
	"A truly general-purpose composite widget is a large, complex piece 
	of software.  You should leave this programming to the widget writers
	who write commercial widget sets, and concentrate on things that are
	more important in your application."
 */

#include <stdio.h>
#include <stdlib.h>
#include "nsXtManageWidget.h"
#include "nsXtManageWidgetP.h"

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply);

static void
ChangeManaged(Widget w);


extern void nsWindow_ResizeWidget(Widget w);


NewManageClassRec newManageClassRec =
{
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &xmManagerClassRec,
    /* class_name         */    "NewManage",
    /* widget_size        */    sizeof(NewManageRec),
    /* class_initialize   */    NULL,
    /* class_partinit     */    NULL /* ClassPartInitialize */,
    /* class_inited       */	FALSE,
    /* initialize         */    NULL /* Initialize */,
    /* Init hook	  */    NULL,
    /* realize            */    XtInheritRealize /* Realize */,
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
    /* resize             */    nsWindow_ResizeWidget,
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
    /* geometry_manager   */    GeometryManager /*(XtGeometryHandler )GeometryManager*/,
    /* change_managed     */    ChangeManaged /*(XtWidgetProc) ChangeManaged*/,
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
/* Manager Class */
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
/* NewManage class - none */     
     /* mumble */               0
 }
};


WidgetClass newManageClass = (WidgetClass)&newManageClassRec;


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
