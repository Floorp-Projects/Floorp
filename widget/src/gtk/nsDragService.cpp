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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) 1998 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s):
 * Christopher Blizzard <blizzard@mozilla.org>
*/

#include "nsDragService.h"
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsString.h"
#include <gtk/gtkinvisible.h>
#include <gdk/gdkx.h>

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#undef DEBUG_DD

GtkTargetList *targetListFromTransArr(nsISupportsArray *anArray);

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE3(nsDragService, nsIDragService, nsIDragSession, nsIDragSessionGTK)

// these are callbacks for our invisible widget
static void invisibleDragEnd         (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      gpointer data);
static void invisibleDragDataGet     (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      GtkSelectionData *selection_data,
                                      guint             info,
                                      guint32           time,
                                      gpointer          data);

nsDragService::nsDragService()
{
  // set up our invisible widget
  mHiddenWidget   = gtk_invisible_new();
  // make sure that the widget is realized so that
  // we can use it as a drag source.
  gtk_widget_realize(mHiddenWidget);
  // hook up the right signals to the hidden widget
  gtk_signal_connect(GTK_OBJECT(mHiddenWidget), "drag_data_get",
                     GTK_SIGNAL_FUNC(invisibleDragDataGet), NULL);
  gtk_signal_connect(GTK_OBJECT(mHiddenWidget), "drag_end",
                     GTK_SIGNAL_FUNC(invisibleDragEnd), NULL);
  // make sure to set these two vars to zero otherwise the reset call
  // will try to free them.
  mDataItems = 0;
  mDragData = 0;
  // set up our state
  mDoingDrag = PR_FALSE;
  // reset everything
  ResetDragState();
  mTimeCB = nsnull;
}

nsDragService::~nsDragService()
{
  ResetDragState();
  gtk_widget_destroy(mHiddenWidget);
}

// nsIDragService
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables,
                                                nsIScriptableRegion * aRegion,
                                                PRUint32 aActionType)
{
#ifdef DEBUG_DD
  g_print("InvokeDragSession\n");
#endif
  // make sure that we have an array of transferables to use
  if (!anArrayTransferables)
    return NS_ERROR_INVALID_ARG;
  // set our reference to the transferables.  this will also addref
  // the transferables since we're going to hang onto this beyond the
  // length of this call
  SetDataItems(anArrayTransferables);
  // make sure that there isn't a context for this drag, yet.
  SetLastContext(nsnull, nsnull, 0);
  // create a target list from the list of transferables
  GtkTargetList *targetList = targetListFromTransArr(anArrayTransferables);
  if (targetList) {
    // get the last time event.  we do this because if we don't then
    // gdk_drag_begin() will use the current time as the arg for the
    // grab.  if you happen to do a drag really quickly and release
    // the mouse button before the drag begins ( really easy to do, by
    // the way ) then the server ungrab from the mouse button release
    // will actually have a time that is _before_ the server grab that
    // we are about to cause and it will leave the server in a grabbed
    // state after the drag has ended.
    guint32 last_event_time = 0;
    mTimeCB(&last_event_time);
    // synth an event so that that fun bug in the gtk dnd code doesn't
    // rear its ugly head
    GdkEvent gdk_event;
    gdk_event.type = GDK_BUTTON_PRESS;
    gdk_event.button.window = mHiddenWidget->window;
    gdk_event.button.send_event = 0;
    gdk_event.button.time = last_event_time;
    gdk_event.button.x = 0;
    gdk_event.button.y = 0;
    gdk_event.button.pressure = 0;
    gdk_event.button.xtilt = 0;
    gdk_event.button.ytilt = 0;
    gdk_event.button.state = 0;
    gdk_event.button.button = 0;
    gdk_event.button.source = (GdkInputSource)0;
    gdk_event.button.deviceid = 0;
    gdk_event.button.x_root = 0;
    gdk_event.button.y_root = 0;

    // start our drag.
    GdkDragContext *context = gtk_drag_begin(mHiddenWidget,
                                             targetList,
                                             GDK_ACTION_DEFAULT,
                                             1,
                                             &gdk_event);
    // make sure to set our default icon
    gtk_drag_set_icon_default (context);
    gtk_target_list_unref(targetList);
    // set our last context as this context
    SetLastContext(mHiddenWidget, context, gdk_time_get());
  }

  return NS_OK;
}

NS_IMETHODIMP nsDragService::StartDragSession()
{
  // someone just started a drag.  clean up first.
  ResetDragState();
  mDoingDrag = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::EndDragSession()
{
  // a drag just ended.  reset everything.
  ResetDragState();
  return NS_OK;
}

// nsIDragSession
NS_IMETHODIMP nsDragService::SetCanDrop            (PRBool           aCanDrop)
{
#ifdef DEBUG_DD
  g_print("can drop: %d\n", aCanDrop);
#endif
  mCanDrop = aCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetCanDrop            (PRBool          *aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetNumDropItems       (PRUint32 * aNumItems)
{
  if (!aNumItems)
    return NS_ERROR_INVALID_ARG;

  // we are lame.  we can only do one at a time at this point.
  // XXX according to pink, windows does some sort of encoding for multiple items.
  // we need to do something similar.
  *aNumItems = 1;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetData               (nsITransferable * aTransferable, PRUint32 anItemIndex)
{
  nsresult errCode = NS_ERROR_FAILURE;
#ifdef DEBUG_DD
  printf("nsDragService::GetData\n");
#endif

  // make sure that we have a transferable
  if (!aTransferable)
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion). Flavors are nsISupportsStrings so that they can be seen from JS.
  nsCOMPtr<nsISupportsArray> flavorList;
  errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return errCode;

  // Now walk down the list of flavors. When we find one that is actually present,
  // copy out the data into the transferable in that format. SetTransferData()
  // implicitly handles conversions.
  PRUint32 cnt;
  flavorList->Count ( &cnt );
  for ( unsigned int i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericWrapper;
    flavorList->GetElementAt ( i, getter_AddRefs(genericWrapper) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericWrapper) );
    if ( currentFlavor ) {
      // find our gtk flavor
      nsXPIDLCString flavorStr;
      currentFlavor->ToString ( getter_Copies(flavorStr) );
      GdkAtom gdkFlavor = gdk_atom_intern(flavorStr, FALSE);
#ifdef DEBUG_DD
      printf("looking for data in type %s, gdk flavor %ld\n", NS_STATIC_CAST(const char*,flavorStr), gdkFlavor);
#endif
      PRBool dataFound = PR_FALSE;
      if (gdkFlavor) {
        // XXX check the return value here
        GetNativeDragData(gdkFlavor);
      }
      if (mDragData) {
        dataFound = PR_TRUE;
      }
      else {
        // if we are looking for text/unicode and we fail to find it on the clipboard first,
        // try again with text/plain. If that is present, convert it to unicode.
        if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
          gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
          GetNativeDragData(gdkFlavor);
          if (mDragData) {
            const char* castedText = NS_REINTERPRET_CAST(char*, mDragData);
            PRUnichar* convertedText = nsnull;
            PRInt32 convertedTextLen = 0;
            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, mDragDataLen, 
                                                                        &convertedText, &convertedTextLen );
            if ( convertedText ) {
              // out with the old, in with the new 
              g_free(mDragData);
              mDragData = convertedText;
              mDragDataLen = convertedTextLen * 2;
              dataFound = PR_TRUE;
            } // if plain text data on clipboard
          } // if plain text flavor present
        } // if looking for text/unicode   
      } // else we try one last ditch effort to find our data

      if ( dataFound ) {
        // the DOM only wants LF, so convert from MacOS line endings to DOM line
        // endings.
        nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr,
                                                             &mDragData,
                                                             NS_REINTERPRET_CAST(int*, &mDragDataLen) );
        
        // put it into the transferable.
        nsCOMPtr<nsISupports> genericDataWrapper;
        nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, mDragData, mDragDataLen, getter_AddRefs(genericDataWrapper) );
        errCode = aTransferable->SetTransferData ( flavorStr, genericDataWrapper, mDragDataLen );
        #ifdef NS_DEBUG
         if ( errCode != NS_OK ) printf("nsDragService:: Error setting data into transferable\n");
        #endif
          
        errCode = NS_OK;

        // we found one, get out of this loop!
        break;
      } 
    }
  } // foreach flavor
  
  return errCode;
}

NS_IMETHODIMP nsDragService::IsDataFlavorSupported (const char *aDataFlavor, PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;

  // check to make sure that we have a drag object set, here
  if (!mLastContext) {
#ifdef DEBUG_DD
    g_print("*** warning: IsDataFlavorSupported called without a valid drag context!\n");
#endif
    return *_retval = PR_FALSE;
  }

#ifdef DEBUG_DD
  g_print("isDataFlavorSupported: %s\n", aDataFlavor);
#endif

  GList *tmp;

  for (tmp = mLastContext->targets; tmp; tmp = tmp->next) {
    GdkAtom atom = GPOINTER_TO_INT(tmp->data);
    gchar *name = NULL;
    name = gdk_atom_name(atom);
#ifdef DEBUG_DD
    g_print("checking %s against %s\n", name, aDataFlavor);
#endif
    if (strcmp(name, aDataFlavor) == 0) {
#ifdef DEBUG_DD
      g_print("boioioioiooioioioing!\n");
#endif
      *_retval = PR_TRUE;
    }
    g_free(name);
  }

  return NS_OK;
}

// nsIDragSessionGTK
NS_IMETHODIMP nsDragService::SetLastContext  (GtkWidget          *aWidget,
                                              GdkDragContext     *aContext,
                                              guint               aTime)
{
  mLastWidget = aWidget;
  mLastContext = aContext;
  if (aContext) {
#ifdef DEBUG_DD
    g_print("doing drag...\n");
#endif
    mDoingDrag = PR_TRUE;
  }
  else {
#ifdef DEBUG_DD
    g_print("not doing drag...\n");
#endif
    mDoingDrag = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDragService::StartDragMotion(GtkWidget      *aWidget,
                                             GdkDragContext *aContext,
                                             guint           aTime)
{
#ifdef DEBUG_DD
  g_print("StartDragMotion\n");
#endif
  // set our drop target to false since we probably won't get
  // notification if there's no JS DND listener.
  mCanDrop = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::EndDragMotion(GtkWidget      *aWidget,
                                           GdkDragContext *aContext,
                                           guint           aTime)
{
#ifdef DEBUG_DD
  g_print("EndDragMotion: %d\n", mCanDrop);
#endif
  if (mCanDrop) 
    gdk_drag_status(aContext, GDK_ACTION_COPY, aTime);
  else
    gdk_drag_status(aContext, (GdkDragAction)0, mLastTime);
  return NS_OK;
}

NS_IMETHODIMP nsDragService::SetDataReceived (GtkWidget          *aWidget,
                                              GdkDragContext     *context,
                                              gint                x,
                                              gint                y,
                                              GtkSelectionData   *selection_data,
                                              guint               info,
                                              guint32             time)
{
  mDataReceived = PR_TRUE;
  // make sure to free old drag data
  if (mDragData)
    g_free(mDragData);
  // ...and set our length to zero by default
  mDragDataLen = 0;
  // see if we can pull the data out
  if (selection_data->length > 0) {
    mDragDataLen = selection_data->length;
    mDragData = g_malloc(mDragDataLen);
    memcpy(mDragData, selection_data->data, mDragDataLen);
  }
  else {
#ifdef DEBUG_DD
    g_print("failed to get data. selection_data->length was %d\n", selection_data->length);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP nsDragService::DataGetSignal         (GtkWidget          *widget,
                                                    GdkDragContext     *context,
                                                    GtkSelectionData   *selection_data,
                                                    guint               info,
                                                    guint32             time,
                                                    gpointer            data)
{
  
  nsCOMPtr<nsISupports> genericItem;
  // XXX we are lame.  it's always 0.
  if (!mDataItems) {
#ifdef DEBUG_DD
    g_print("Failed to get our data items\n");
#endif
    ResetDragState();
    return NS_ERROR_FAILURE;
  }

  mDataItems->GetElementAt(0, getter_AddRefs(genericItem));
  nsCOMPtr<nsITransferable> item (do_QueryInterface(genericItem));
  if (item) {
    nsCAutoString mimeFlavor;

    GdkAtom atom = info;
    gchar *type_name = NULL;
    type_name = gdk_atom_name(atom);
    // this makes a copy...
    mimeFlavor = type_name;
    g_free(type_name);
    if (!type_name) {
#ifdef DEBUG_DD
      g_print("failed to get atom name.\n");
#endif
      ResetDragState();
      return NS_ERROR_FAILURE;
    }
    // if someone was asking for text/plain, lookup unicode instead so we can convert it.
    PRBool needToDoConversionToPlainText = PR_FALSE;
    char* actualFlavor = mimeFlavor;
    if ( strcmp(mimeFlavor,kTextMime) == 0 ) {
      actualFlavor = kUnicodeMime;
      needToDoConversionToPlainText = PR_TRUE;
    }
    else
      actualFlavor = mimeFlavor;

    PRUint32 tmpDataLen = 0;
    void    *tmpData = NULL;
    nsCOMPtr<nsISupports> data;
    if ( NS_SUCCEEDED(item->GetTransferData(actualFlavor, getter_AddRefs(data), &tmpDataLen)) ) {
      nsPrimitiveHelpers::CreateDataFromPrimitive ( actualFlavor, data, &tmpData, tmpDataLen );
      // if required, do the extra work to convert unicode to plain text and replace the output
      // values with the plain text.
      if ( needToDoConversionToPlainText ) {
        char* plainTextData = nsnull;
        PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, tmpData);
        PRInt32 plainTextLen = 0;
        nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, 
                                                                tmpDataLen / 2, 
                                                                &plainTextData, &plainTextLen );
        if (tmpData) {
          g_free(tmpData);
          tmpData = plainTextData;
          tmpDataLen = plainTextLen;
        }
      }
      if ( tmpData ) {
        // this copies the data
        gtk_selection_data_set(selection_data, selection_data->target,
                               8, (guchar *)tmpData, tmpDataLen);
        g_free(tmpData);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsDragService::SetTimeCallback (nsIDragSessionGTKTimeCB aCallback)
{
#ifdef DEBUG_DD
  g_print("SetTimeCallback %p\n", aCallback);
#endif
  mTimeCB = aCallback;
  return NS_OK;
}

void nsDragService::ResetDragState(void)
{
  // make sure that all of our last state is set
  mLastContext = NULL;
  mLastWidget = NULL;
  mLastTime = 0;
  // make sure that our data is uninitialized
  NS_IF_RELEASE(mDataItems);
  mDataItems = nsnull;
  // our drag data
  mDataReceived = PR_FALSE;
  if (mDragData) {
    g_free(mDragData);
    mDragData = NULL;
  }
  mDragDataLen = 0;
  mCanDrop = PR_FALSE;
  mDoingDrag = PR_FALSE;
}

void nsDragService::SetDataItems(nsISupportsArray *anArray)
{
  NS_IF_RELEASE(mDataItems);
  mDataItems = anArray;
  NS_ADDREF(mDataItems);
}

NS_METHOD nsDragService::GetNativeDragData(GdkAtom aFlavor)
{
  gtk_grab_add(mHiddenWidget);
  gtk_drag_get_data(mLastWidget, mLastContext, aFlavor, mLastTime);
  while (mDataReceived == PR_FALSE && mDoingDrag) {
    // XXX check the number of iterations...we could grab forever and
    // that would make me sad.
    gtk_main_iteration();
  }
#ifdef DEBUG_DD
  g_print("got data\n");
#endif
  gtk_grab_remove(mHiddenWidget);
  return NS_OK;
}

static void invisibleDragEnd     (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  gpointer data)
{
#ifdef DEBUG_DD
  g_print("invisbleDragEnd\n");
#endif
  // apparently, the drag is over.  make sure to tell the drag service
  // about it.
  nsCOMPtr<nsIDragService> dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             NS_GET_IID(nsIDragService),
                                             (nsISupports **)&dragService);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_DD
    g_print("*** warning: failed to get the drag service. this is a _bad_ thing.\n");
#endif
    return;
  }
  dragService->EndDragSession();
}

static void invisibleDragDataGet (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint32           aTime,
                                  gpointer          data)
{
#ifdef DEBUG_DD
  g_print("invisibleDragDataGet\n");
#endif

  nsCOMPtr<nsIDragService> dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_DD
    g_print("*** warning: failed to get the drag service. this is a _bad_ thing.\n");
#endif
    return;
  }
  nsCOMPtr<nsIDragSessionGTK> dragServiceGTK;
  dragServiceGTK = do_QueryInterface(dragService);
  if (!dragServiceGTK) {
#ifdef DEBUG_DD
    g_print("oops\n");
#endif
    return;
  }

  dragServiceGTK->DataGetSignal(widget, context, selection_data, info, aTime, data);
}

// this function will take a lit of drag item flavors and
// build a valid target list from it.

GtkTargetList *targetListFromTransArr(nsISupportsArray *inArray)
{
  if (inArray == nsnull)
    return NULL;
  nsVoidArray targetArray;
  GtkTargetEntry *targets;
  GtkTargetList  *targetList;
  PRUint32 targetCount = 0;
  unsigned int numDragItems = 0;

  inArray->Count(&numDragItems);
  for (unsigned int itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
    nsCOMPtr<nsISupports> genericItem;
    inArray->GetElementAt(itemIndex, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
    if (currItem) {
      nsCOMPtr <nsISupportsArray> flavorList;
      if ( NS_SUCCEEDED(currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList))) ) {
        PRUint32 numFlavors;
        flavorList->Count( &numFlavors );
        for ( unsigned int flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex ) {
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt (flavorIndex, getter_AddRefs(genericWrapper));
          nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericWrapper) );
          if (currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString ( getter_Copies(flavorStr) );
            // get the atom
            GdkAtom atom = gdk_atom_intern(flavorStr, FALSE);
            GtkTargetEntry *target = (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
            target->target = g_strdup(flavorStr);
            target->flags = 0;
            target->info = atom;
#ifdef DEBUG_DD
            g_print("adding target %s with id %ld\n", target->target, atom);
#endif
            targetArray.AppendElement(target);
          }
        } // foreach flavor in item              
      } // if valid flavor list
    } // if item is a transferable
  }

  // get all the elements that we created.
  targetCount = targetArray.Count();
  if (targetCount) {
    // allocate space to create the list of valid targets
    targets = (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry) * targetCount);
    for (PRUint32 targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
      GtkTargetEntry *disEntry = (GtkTargetEntry *)targetArray.ElementAt(targetIndex);
      // this is a string reference but it will be freed later.
      targets[targetIndex].target = disEntry->target;
      targets[targetIndex].flags = disEntry->flags;
      targets[targetIndex].info = disEntry->info;
    }
    targetList = gtk_target_list_new(targets, targetCount);
    // clean up the target list
    for (PRUint32 cleanIndex = 0; cleanIndex < targetCount; ++cleanIndex) {
      GtkTargetEntry *thisTarget = (GtkTargetEntry *)targetArray.ElementAt(cleanIndex);
      g_free(thisTarget->target);
      g_free(thisTarget);
    }
  }

  return targetList;
}

