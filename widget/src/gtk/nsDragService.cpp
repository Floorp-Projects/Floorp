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

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession)

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


//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray *anArrayTransferables,
                                                nsIScriptableRegion *aRegion,
                                                PRUint32 aActionType)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  *aNumItems = 0;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable, PRUint32 anItem)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetCurrentSession (nsIDragSession **aSession)
{
  return NS_ERROR_FAILURE;
}




















//-------------------------------------------------------------------------
void  
nsDragService::DragLeave (GtkWidget	       *widget,
			                    GdkDragContext   *context,
			                    guint             time)
{
  g_print("leave\n");
  //gHaveDrag = PR_FALSE;
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
      gHaveDrag = PR_TRUE;
  }
#endif

  source_widget = gtk_drag_get_source_widget (context);
  g_print("motion, source %s\n", source_widget ?
	          gtk_type_name (GTK_OBJECT (source_widget)->klass->type) :
	          "unknown");

  gdk_drag_status (context, context->suggested_action, time);
  
  return PR_TRUE;
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
  //gHaveDrag = PR_FALSE;

  if (context->targets){
    gtk_drag_get_data (widget, context, 
			                 GPOINTER_TO_INT (context->targets->data), 
			                 time);
    return PR_TRUE;
  }
  
  return PR_FALSE;
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
    gtk_drag_finish (context, PR_TRUE, PR_FALSE, time);
    return;
  }
  
  gtk_drag_finish (context, PR_FALSE, PR_FALSE, time);
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
