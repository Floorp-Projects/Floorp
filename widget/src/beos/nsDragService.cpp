/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsIServiceManager.h"

#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);
static NS_DEFINE_CID(kCDragServiceCID,   NS_DRAGSERVICE_CID);

// The class statics:
BView *nsDragService::sView = 0;

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)

//-------------------------------------------------------------------------
// static variables
//-------------------------------------------------------------------------
  //static PRBool gHaveDrag = PR_FALSE;

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

  if (aIID.Equals(NS_GET_IID(nsIDragService))) {
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
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable,
                                        PRUint32 aItemIndex)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
void nsDragService::SetTopLevelView(BView *v)
{
  printf("  nsDragService::SetTopLevelWidget\n");
  
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sView == v)
    return;

  sView = v;

  // Get the DragService from the service manager.
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService(do_GetService(kCDragServiceCID, &rv));

  if (NS_FAILED(rv)) {
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

}

//-------------------------------------------------------------------------
//void  
//nsDragService::DragLeave (GtkWidget	       *widget,
//			                    GdkDragContext   *context,
//			                    guint             time)
//{
//  g_print("leave\n");
//  //gHaveDrag = PR_FALSE;
//}
//
////-------------------------------------------------------------------------
//PRBool
//nsDragService::DragMotion(GtkWidget	       *widget,
//			                    GdkDragContext   *context,
//			                    gint              x,
//			                    gint              y,
//			                    guint             time)
//{
//  g_print("drag motion\n");
//  GtkWidget *source_widget;
//
//#if 0
//  if (!gHaveDrag) {
//      gHaveDrag = PR_TRUE;
//  }
//#endif
//
//  source_widget = gtk_drag_get_source_widget (context);
//  g_print("motion, source %s\n", source_widget ?
//	          gtk_type_name (GTK_OBJECT (source_widget)->klass->type) :
//	          "unknown");
//
//  gdk_drag_status (context, context->suggested_action, time);
//  
//  return PR_TRUE;
//}
//
////-------------------------------------------------------------------------
//PRBool
//nsDragService::DragDrop(GtkWidget	       *widget,
//			                  GdkDragContext   *context,
//			                  gint              x,
//			                  gint              y,
//			                  guint             time)
//{
//  g_print("drop\n");
//  //gHaveDrag = PR_FALSE;
//
//  if (context->targets){
//    gtk_drag_get_data (widget, context, 
//			                 GPOINTER_TO_INT (context->targets->data), 
//			                 time);
//    return PR_TRUE;
//  }
//  
//  return PR_FALSE;
//}
//
////-------------------------------------------------------------------------
//void  
//nsDragService::DragDataReceived  (GtkWidget          *widget,
//			                            GdkDragContext     *context,
//			                            gint                x,
//			                            gint                y,
//			                            GtkSelectionData   *data,
//			                            guint               info,
//			                            guint               time)
//{
//  if ((data->length >= 0) && (data->format == 8)) {
//    g_print ("Received \"%s\"\n", (gchar *)data->data);
//    gtk_drag_finish (context, PR_TRUE, PR_FALSE, time);
//    return;
//  }
//  
//  gtk_drag_finish (context, PR_FALSE, PR_FALSE, time);
//}
//  
////-------------------------------------------------------------------------
//void  
//nsDragService::DragDataGet(GtkWidget          *widget,
//		                       GdkDragContext     *context,
//		                       GtkSelectionData   *selection_data,
//		                       guint               info,
//		                       guint               time,
//		                       gpointer            data)
//{
//  gtk_selection_data_set (selection_data,
//                          selection_data->target,
//                          8, (guchar *)"I'm Data!", 9);
//}
//
////-------------------------------------------------------------------------
//void  
//nsDragService::DragDataDelete(GtkWidget          *widget,
//			                        GdkDragContext     *context,
//			                        gpointer            data)
//{
//  g_print ("Delete the data!\n");
//}
