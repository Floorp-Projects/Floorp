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

#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsIServiceManager.h"

#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);
static NS_DEFINE_IID(kCDragServiceCID,   NS_DRAGSERVICE_CID);

// The class statics:
GtkWidget* nsDragService::sWidget = 0;

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)

//-------------------------------------------------------------------------
// static variables
//-------------------------------------------------------------------------
  //static PRBool gHaveDrag = FALSE;

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIDragServiceIID)) {
    *aInstancePtr = (void*) ((nsIDragService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}

//---------------------------------------------------------
NS_IMETHODIMP nsDragService::StartDragSession (nsITransferable * aTransferable, PRUint32 aActionType)

{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
void nsDragService::SetTopLevelWidget(GtkWidget* w)
{
  printf("  nsDragService::SetTopLevelWidget\n");
  
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sWidget == w)
    return;

  sWidget = w;

  // Get the DragService from the service manager.
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             kIDragServiceIID,
                                             (nsISupports**)&dragService);
  if (!NS_SUCCEEDED(rv)) {
    return;
  }

#if 0
  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_leave",
		      GTK_SIGNAL_FUNC (nsDragService::DragLeave), dragService);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_motion",
		      GTK_SIGNAL_FUNC (nsDragService::DragMotion), dragService);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_drop",
		      GTK_SIGNAL_FUNC (nsDragService::DragDrop), dragService);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_data_received",
		      GTK_SIGNAL_FUNC (nsDragService::DragDataReceived), dragService);
#endif

  // We're done with our reference to the dragService.
  //NS_IF_RELEASE(dragService);
}

//-------------------------------------------------------------------------
void  
nsDragService::DragLeave (GtkWidget	       *widget,
			                    GdkDragContext   *context,
			                    guint             time)
{
  g_print("leave\n");
  //gHaveDrag = FALSE;
}

//-------------------------------------------------------------------------
PRBool
nsDragService::DragMotion(GtkWidget	       *widget,
			                    GdkDragContext   *context,
			                    gint              x,
			                    gint              y,
			                    guint             time)
{
  g_print("drag motion\n");
  GtkWidget *source_widget;

#if 0
  if (!gHaveDrag) {
      gHaveDrag = TRUE;
  }
#endif

  source_widget = gtk_drag_get_source_widget (context);
  g_print("motion, source %s\n", source_widget ?
	          gtk_type_name (GTK_OBJECT (source_widget)->klass->type) :
	          "unknown");

  gdk_drag_status (context, context->suggested_action, time);
  
  return TRUE;
}

//-------------------------------------------------------------------------
PRBool
nsDragService::DragDrop(GtkWidget	       *widget,
			                  GdkDragContext   *context,
			                  gint              x,
			                  gint              y,
			                  guint             time)
{
  g_print("drop\n");
  //gHaveDrag = FALSE;

  if (context->targets){
    gtk_drag_get_data (widget, context, 
			                 GPOINTER_TO_INT (context->targets->data), 
			                 time);
    return TRUE;
  }
  
  return FALSE;
}

//-------------------------------------------------------------------------
void  
nsDragService::DragDataReceived  (GtkWidget          *widget,
			                            GdkDragContext     *context,
			                            gint                x,
			                            gint                y,
			                            GtkSelectionData   *data,
			                            guint               info,
			                            guint               time)
{
  if ((data->length >= 0) && (data->format == 8)) {
    g_print ("Received \"%s\"\n", (gchar *)data->data);
    gtk_drag_finish (context, TRUE, FALSE, time);
    return;
  }
  
  gtk_drag_finish (context, FALSE, FALSE, time);
}
  
//-------------------------------------------------------------------------
void  
nsDragService::DragDataGet(GtkWidget          *widget,
		                       GdkDragContext     *context,
		                       GtkSelectionData   *selection_data,
		                       guint               info,
		                       guint               time,
		                       gpointer            data)
{
  gtk_selection_data_set (selection_data,
                          selection_data->target,
                          8, (guchar *)"I'm Data!", 9);
}

//-------------------------------------------------------------------------
void  
nsDragService::DragDataDelete(GtkWidget          *widget,
			                        GdkDragContext     *context,
			                        gpointer            data)
{
  g_print ("Delete the data!\n");
}
