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
#include "nsString.h"
#include "nsClipboard.h"
#include "nsIRegion.h"
#include "nsVoidArray.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsWidgetsCID.h"

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession)

#define DEBUG_DRAG 1

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
  mNumFlavors = 0;
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
}

enum {
  TARGET_STRING,
  TARGET_ROOTWIN
};

static GtkTargetEntry target_table[] = {
  { "STRING",     0, TARGET_STRING },
  { "text/plain", 0, TARGET_STRING },
  { "application/x-rootwin-drop", 0, TARGET_ROOTWIN }
};

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);


NS_IMETHODIMP nsDragService::StartDragSession()
{
  printf("nsDragService::StartDragSession()\n");
  nsBaseDragService::StartDragSession();
  
  /*
  gtk_drag_source_set(mWidget,
                      GDK_MODIFIER_MASK,
                      targetlist,
                      mNumFlavors,
                      action);
  */
  gtk_drag_source_set(mWidget,
                      GDK_MODIFIER_MASK,
                      target_table,
                      n_targets,
                      mActionType);

  return NS_OK;
}

NS_IMETHODIMP nsDragService::EndDragSession()
{
  printf("nsDragService::EndDragSession()\n");
  nsBaseDragService::EndDragSession();
  
  gtk_drag_source_unset(mWidget);

  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray *aTransferableArray,
                                                nsIScriptableRegion *aRegion,
                                                PRUint32 aActionType)
{
  mWidget = gtk_get_event_widget(gtk_get_current_event());

  // add the flavors from the transferables. Cache this array for the send data proc
  GtkTargetList *targetlist = RegisterDragItemsAndFlavors(aTransferableArray);

  switch (aActionType)
  {
    case DRAGDROP_ACTION_NONE:
      mActionType = GDK_ACTION_DEFAULT;
      break;
    case DRAGDROP_ACTION_COPY:
      mActionType = GDK_ACTION_COPY;
      break;
    case DRAGDROP_ACTION_MOVE:
      mActionType = GDK_ACTION_MOVE;
      break;
    case DRAGDROP_ACTION_LINK:
      mActionType = GDK_ACTION_LINK;
      break;
  }

  StartDragSession();

  // XXX 3rd param ???    &                last param should be a real event...
  gtk_drag_begin(mWidget, targetlist, mActionType, 1, gtk_get_current_event());




#if 0  
  // we have to synthesize the native event because we may be called from JavaScript
  // through XPConnect. In that case, we only have a DOM event and no way to
  // get to the native event. As a consequence, we just always fake it.
  Point globalMouseLoc;
  ::GetMouse(&globalMouseLoc);
  ::LocalToGlobal(&globalMouseLoc);
  WindowPtr theWindow = nsnull;
  if ( ::FindWindow(globalMouseLoc, &theWindow) != inContent ) {
    // debugging sanity check
    #ifdef NS_DEBUG
    DebugStr("\pAbout to start drag, but FindWindow() != inContent; g");
    #endif
  }  
  EventRecord theEvent;
  theEvent.what = mouseDown;
  theEvent.message = reinterpret_cast<UInt32>(theWindow);
  theEvent.when = 0;
  theEvent.where = globalMouseLoc;
  theEvent.modifiers = 0;
  
  RgnHandle theDragRgn = ::NewRgn();
  BuildDragRegion ( aDragRgn, globalMouseLoc, theDragRgn );

  // register drag send proc which will call us back when asked for the actual
  // flavor data (instead of placing it all into the drag manager)
  ::SetDragSendProc ( theDragRef, sDragSendDataUPP, this );

  // start the drag. Be careful, mDragRef will be invalid AFTER this call (it is
  // reset by the dragTrackingHandler).
  ::TrackDrag ( theDragRef, &theEvent, theDragRgn );

  // clean up after ourselves 
  ::DisposeRgn ( theDragRgn );
  result = ::DisposeDrag ( theDragRef );
  NS_ASSERTION ( result == noErr, "Error disposing drag" );
  mDragRef = 0L;
  mDataItems = nsnull;

  return NS_OK; 

#endif




  return NS_OK;
}




GtkTargetList *
nsDragService::RegisterDragItemsAndFlavors(nsISupportsArray *inArray)
{
  unsigned int numDragItems = 0;
  inArray->Count(&numDragItems);

  GtkTargetList *targetlist;
  targetlist = gtk_target_list_new(nsnull, numDragItems);

  for (unsigned int i = 0; i < numDragItems; ++i)
  {
    nsCOMPtr<nsISupports> genericItem;
    inArray->GetElementAt (i, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
    if (currItem)
    {
      nsCOMPtr<nsISupportsArray> flavorList;
      if (NS_SUCCEEDED(currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList))))
      {
        flavorList->Count (&mNumFlavors);
        for (PRUint32 flavorIndex = 0; flavorIndex < mNumFlavors; ++flavorIndex)
        {
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt ( flavorIndex, getter_AddRefs(genericWrapper) );
          nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericWrapper) );
          if ( currentFlavor )
          {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString ( getter_Copies(flavorStr) );

            // register native flavors
            GdkAtom atom = gdk_atom_intern(flavorStr, PR_TRUE);
            gtk_target_list_add(targetlist, atom, 1, atom);
          }

        } // foreach flavor in item              
      } // if valid flavor list
    } // if item is a transferable
  } // foreach drag item

  return targetlist;
}


/* return PR_TRUE if we have converted or PR_FALSE if we havn't and need to keep being called */
PRBool nsDragService::DoConvert(GdkAtom type)
{
#ifdef DEBUG_DRAG
  g_print("    nsDragService::DoRealConvert(%li)\n    {\n", type);
#endif
  int e = 0;
  // Set a flag saying that we're blocking waiting for the callback:
  mBlocking = PR_TRUE;

  //
  // ask X what kind of data we can get
  //
#ifdef DEBUG_DRAG
  g_print("     Doing real conversion of atom type '%s'\n", gdk_atom_name(type));
#endif
  gtk_selection_convert(mWidget,
                        GDK_SELECTION_PRIMARY,
                        type,
                        GDK_CURRENT_TIME);

  // Now we need to wait until the callback comes in ...
  // i is in case we get a runaway (yuck).
#ifdef DEBUG_DRAG
  printf("      Waiting for the callback... mBlocking = %d\n", mBlocking);
#endif /* DEBUG_CLIPBOARD */
  for (e=0; mBlocking == PR_TRUE && e < 1000; ++e)
  {
    gtk_main_iteration_do(PR_TRUE);
  }

#ifdef DEBUG_DRAG
  g_print("    }\n");
#endif

  if (mSelectionData.length > 0)
    return PR_TRUE;

  return PR_FALSE;
}

#if 0

/**
 * Called when the data from a drag comes in (recieved from gdk_selection_convert)
 *
 * @param  aWidget the widget
 * @param  aSelectionData gtk selection stuff
 * @param  aTime time the selection was requested
 */
void
nsDragService::SelectionReceivedCB (GtkWidget        *aWidget,
                                    GdkDragContext   *aContext,
                                    gint              aX,
                                    gint              aY,
                                    GtkSelectionData *aSelectionData,
                                    guint             aInfo,
                                    guint             aTime)
{
#ifdef DEBUG_DRAG
  printf("      nsDragService::SelectionReceivedCB\n      {\n");
#endif
  nsDragService *ds =(nsDragSession *)gtk_object_get_data(GTK_OBJECT(aWidget),
                                                        "ds");
  if (!cb)
  {
    g_print("no dragservice found.. this is bad.\n");
    return;
  }


  ds->SelectionReceiver(aWidget, aSelectionData);
#ifdef DEBUG_DRAG
  g_print("      }\n");
#endif
}


/**
 * local method (called from nsClipboard::SelectionReceivedCB)
 *
 * @param  aWidget the widget
 * @param  aSelectionData gtk selection stuff
 */
void
nsDragService::SelectionReceiver (GtkWidget *aWidget,
                                GtkSelectionData *aSD)
{
  gint type;

  mBlocking = PR_FALSE;

  if (aSD->length < 0)
  {
    printf("        Error retrieving selection: length was %d\n",
           aSD->length);
    return;
  }


  switch (type)
  {
  case GDK_TARGET_STRING:
  case TARGET_COMPOUND_TEXT:
  case TARGET_TEXT_PLAIN:
  case TARGET_TEXT_XIF:
  case TARGET_TEXT_UNICODE:
  case TARGET_TEXT_HTML:
#ifdef DEBUG_CLIPBOARD
    g_print("        Copying mSelectionData pointer -- ");
#endif
    mSelectionData = *aSD;
    mSelectionData.data = g_new(guchar, aSD->length + 1);
#ifdef DEBUG_CLIPBOARD
    g_print("        Data = %s\n    Length = %i\n", aSD->data, aSD->length);
#endif
    memcpy(mSelectionData.data,
           aSD->data,
           aSD->length);
    // Null terminate in case anyone cares,
    // and so we can print the string for debugging:
    mSelectionData.data[aSD->length] = '\0';
    mSelectionData.length = aSD->length;
    return;

  default:
    mSelectionData = *aSD;
    mSelectionData.data = g_new(guchar, aSD->length + 1);
    memcpy(mSelectionData.data,
           aSD->data,
           aSD->length);
    mSelectionData.length = aSD->length;
    return;
  }
}

#endif

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  *aNumItems = 0;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable, PRUint32 anItem)
{

#ifdef DEBUG_DRAG
  printf("nsClipboard::GetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (!aTransferable) {
    printf("  GetData: Transferable is null!\n");
    return NS_ERROR_FAILURE;
  }

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  // Walk through flavors and see which flavor matches the one being pasted:
  PRUint32 cnt;
  flavorList->Count(&cnt);
  nsCAutoString foundFlavor;
  for ( PRUint32 i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor (do_QueryInterface(genericFlavor));
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      if (DoConvert(gdk_atom_intern(flavorStr, 1))) {
        foundFlavor = flavorStr;
        break;
      }
    }
  }

#ifdef DEBUG_CLIPBOARD
  printf("  Got the callback: '%s', %d\n",
         mSelectionData.data, mSelectionData.length);
#endif /* DEBUG_CLIPBOARD */

  // We're back from the callback, no longer blocking:
  mBlocking = PR_FALSE;

  // 
  // Now we have data in mSelectionData.data.
  // We just have to copy it to the transferable.
  // 

#if 0  
// pinkerton - we have the flavor already from above, so we don't need
// to re-derrive it.
  nsString *name = new nsString((const char*)gdk_atom_name(mSelectionData.type));
  int format = GetFormat(*name);
  df->SetString((const char*)gdk_atom_name(sSelTypes[format]));
#endif

  nsCOMPtr<nsISupports> genericDataWrapper;
  CreatePrimitiveForData ( foundFlavor, mSelectionData.data, mSelectionData.length, getter_AddRefs(genericDataWrapper) );
  aTransferable->SetTransferData(foundFlavor,
                                 genericDataWrapper,
                                 mSelectionData.length);

  //delete name;
  
  // transferable is now copying the data, so we can free it.
  //  g_free(mSelectionData.data);
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;

  gtk_drag_source_unset(mWidget);

  return NS_OK;
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
