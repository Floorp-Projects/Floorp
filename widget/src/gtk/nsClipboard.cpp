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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
 */

#include "nsClipboard.h"

#include "nsCOMPtr.h"
#include "nsFileSpec.h"

#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsITransferable.h"   // kTextMime
#include "nsISupportsPrimitives.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

#include "nsTextFormatter.h"
#include "nsVoidArray.h"


// The class statics:
GtkWidget* nsClipboard::sWidget = 0;


#if defined(DEBUG_mcafee) || defined(DEBUG_pavlov)
#define DEBUG_CLIPBOARD
#endif

static GdkAtom GDK_SELECTION_CLIPBOARD;

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::nsClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  //NS_INIT_REFCNT();
  mIgnoreEmptyNotification = PR_FALSE;
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;
  mSelectionAtom = GDK_SELECTION_PRIMARY;

  // initialize the widget, etc we're binding to
  Init();
}

// XXX if GTK's internal code changes this isn't going to work
// copied from gtk code because it is a static function we can't get to it.
// need to bug owen taylor about getting this code public.

typedef struct _GtkSelectionTargetList GtkSelectionTargetList;

struct _GtkSelectionTargetList {
  GdkAtom selection;
  GtkTargetList *list;
};

static const char *gtk_selection_handler_key = "gtk-selection-handlers";

void __gtk_selection_target_list_remove (GtkWidget *widget, GdkAtom selection)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list, *tmp_list2;
  GList *lists;
  lists = (GList*)gtk_object_get_data (GTK_OBJECT (widget), gtk_selection_handler_key);
  tmp_list = lists;
  while (tmp_list) {
    sellist = (GtkSelectionTargetList*)tmp_list->data;
    if (sellist->selection == selection) {
      gtk_target_list_unref (sellist->list);
      g_free (sellist);
      tmp_list->data = nsnull;
      tmp_list2 = tmp_list->prev;
      lists = g_list_remove_link(lists, tmp_list);
      g_list_free_1(tmp_list);
      tmp_list = tmp_list2;
    }
    if (tmp_list)
      tmp_list = tmp_list->next;
  }
  gtk_object_set_data(GTK_OBJECT(widget), gtk_selection_handler_key, lists);
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::~nsClipboard()\n");  
#endif /* DEBUG_CLIPBOARD */

  // Remove all our event handlers:
  if (sWidget) {
    if (gdk_selection_owner_get(GDK_SELECTION_PRIMARY) == sWidget->window)
      gtk_selection_remove_all(sWidget);

    if (gdk_selection_owner_get(GDK_SELECTION_CLIPBOARD) == sWidget->window)
      gtk_selection_remove_all(sWidget);
  }

  // free the selection data, if any
  if (mSelectionData.data != nsnull)
    nsAllocator::Free(mSelectionData.data);

  gtk_object_remove_data(GTK_OBJECT(sWidget), "cb");

  if (sWidget) {
    gtk_widget_unref(sWidget);
    sWidget = nsnull;
  }
}


// 
// GTK Weirdness!
// This is here in the hope of being able to call
//  gtk_selection_add_targets(w, GDK_SELECTION_PRIMARY,
//                            targets,
//                            1);
// instead of
//   gtk_selection_add_target(sWidget, 
//                            GDK_SELECTION_PRIMARY,
//                            GDK_SELECTION_TYPE_STRING,
//                            GDK_SELECTION_TYPE_STRING);
// but it turns out that this changes the whole gtk selection model;
// when calling add_targets copy uses selection_clear_event and the
// data structure needs to be filled in in a way that we haven't
// figured out; when using add_target copy uses selection_get and
// the data structure is already filled in as much as it needs to be.
// Some gtk internals wizard will need to solve this mystery before
// we can use add_targets().
//static GtkTargetEntry targets[] = {
//  { "strings n stuff", GDK_SELECTION_TYPE_STRING, GDK_SELECTION_TYPE_STRING }
//};
//

//-------------------------------------------------------------------------
void nsClipboard::Init(void)
{
#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::Init\n");
#endif

  GDK_SELECTION_CLIPBOARD         = gdk_atom_intern("CLIPBOARD", FALSE);

  // create invisible widget to use for the clipboard
  sWidget = gtk_invisible_new();

  // add the clipboard pointer to the widget so we can get it.
  gtk_object_set_data(GTK_OBJECT(sWidget), "cb", this);

  // Handle selection requests if we called gtk_selection_add_target:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_get",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionGetCB),
                     nsnull);

  // When someone else takes the selection away:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_clear_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionClearCB),
                     nsnull);

  // Set up the paste handler:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_received",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionReceivedCB),
                     nsnull);
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  SetWhichClipboard(aWhichClipboard);

  mIgnoreEmptyNotification = PR_TRUE;

#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::SetNativeClipboardData(%i)\n", aWhichClipboard);
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    printf("nsClipboard::SetNativeClipboardData(): no transferable!\n");
    return NS_ERROR_FAILURE;
  }

  // are we already the owner?
  if (gdk_selection_owner_get(mSelectionAtom) == sWidget->window)
  {
    // if so, clear all the targets
    __gtk_selection_target_list_remove(sWidget, mSelectionAtom);
    //    gtk_selection_remove_all(sWidget);
  }

  // we arn't already the owner, so we will become it
  gint have_selection = gtk_selection_owner_set(sWidget,
                                                mSelectionAtom,
                                                GDK_CURRENT_TIME);
  if (have_selection == 0)
    return NS_ERROR_FAILURE;

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  PRUint32 cnt;
  flavorList->Count(&cnt);
  for ( PRUint32 i=0; i<cnt; ++i )
  {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

      // add these types as selection targets
      RegisterFormat(flavorStr, mSelectionAtom);
    }
  }

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}


PRBool nsClipboard::DoRealConvert(GdkAtom type)
{
#ifdef DEBUG_CLIPBOARD
  g_print("    nsClipboard::DoRealConvert(%li)\n    {\n", type);
#endif
  int e = 0;
  // Set a flag saying that we're blocking waiting for the callback:
  mBlocking = PR_TRUE;

  //
  // ask X what kind of data we can get
  //
#ifdef DEBUG_CLIPBOARD
  g_print("     Doing real conversion of atom type '%s'\n", gdk_atom_name(type));
#endif
  gtk_selection_convert(sWidget,
                        mSelectionAtom,
                        type,
                        GDK_CURRENT_TIME);

  gtk_grab_add(sWidget);

  // Now we need to wait until the callback comes in ...
  // i is in case we get a runaway (yuck).
#ifdef DEBUG_CLIPBOARD
  g_print("      Waiting for the callback... mBlocking = %d\n", mBlocking);
#endif /* DEBUG_CLIPBOARD */
  for (e=0; mBlocking == PR_TRUE && e < 1000; ++e)
  {
    gtk_main_iteration_do(PR_TRUE);
  }

  gtk_grab_remove(sWidget);

#ifdef DEBUG_CLIPBOARD
  g_print("    }\n");
#endif

  if (mSelectionData.length > 0)
    return PR_TRUE;

  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// The blocking Paste routine
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable, 
                                    PRInt32 aWhichClipboard)
{
  SetWhichClipboard(aWhichClipboard);

#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::GetNativeClipboardData(%i)\n", aWhichClipboard);
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    printf("  GetNativeClipboardData: Transferable is null!\n");
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
  PRBool foundData = PR_FALSE;
  for ( PRUint32 i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString ( getter_Copies(flavorStr) );
      if (DoConvert(flavorStr)) {
        foundFlavor = flavorStr;
        foundData = PR_TRUE;
        break;
      }
    }
  }
  if ( !foundData ) {
    // if we still haven't found anything yet and we're asked to find text/unicode, then
    // try to give them text plain if it's there.
    if (DoConvert(kTextMime)) {
       const char* castedText = NS_REINTERPRET_CAST(char*, mSelectionData.data);          
       PRUnichar* convertedText = nsnull;
       PRInt32 convertedTextLen = 0;
       nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, mSelectionData.length, 
                                                                 &convertedText, &convertedTextLen );
       if ( convertedText ) {
         // out with the old, in with the new 
         nsAllocator::Free(mSelectionData.data);
         mSelectionData.data = NS_REINTERPRET_CAST(guchar*, convertedText);
         mSelectionData.length = convertedTextLen * 2;
         foundFlavor = kUnicodeMime;
         foundData = PR_TRUE;
       }
     } // if plain text data on clipboard
  }
  
#ifdef DEBUG_CLIPBOARD
  g_print("  Got the callback: '%s', %d\n",
         mSelectionData.data, mSelectionData.length);
#endif /* DEBUG_CLIPBOARD */

  // We're back from the callback, no longer blocking:
  mBlocking = PR_FALSE;

  // 
  // Now we have data in mSelectionData.data.
  // We just have to copy it to the transferable.
  // 

  if ( foundData ) {
    nsCOMPtr<nsISupports> genericDataWrapper;
    nsPrimitiveHelpers::CreatePrimitiveForData(foundFlavor, mSelectionData.data,
                                               mSelectionData.length, getter_AddRefs(genericDataWrapper));
    aTransferable->SetTransferData(foundFlavor,
                                   genericDataWrapper,
                                   mSelectionData.length);
  }
    
  // transferable is now owning the data, so we can free it.
  nsAllocator::Free(mSelectionData.data);
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;

  return NS_OK;
}

/**
 * Called when the data from a paste comes in (recieved from gdk_selection_convert)
 *
 * @param  aWidget the widget
 * @param  aSelectionData gtk selection stuff
 * @param  aTime time the selection was requested
 */
void
nsClipboard::SelectionReceivedCB (GtkWidget        *aWidget,
                                  GtkSelectionData *aSelectionData,
                                  guint             aTime)
{
#ifdef DEBUG_CLIPBOARD
  g_print("      nsClipboard::SelectionReceivedCB\n      {\n");
#endif /* DEBUG_CLIPBOARD */
  nsClipboard *cb =(nsClipboard *)gtk_object_get_data(GTK_OBJECT(aWidget),
                                                      "cb");
  if (!cb)
  {
    g_print("no clipboard found.. this is bad.\n");
    return;
  }
  cb->SelectionReceiver(aWidget, aSelectionData);
#ifdef DEBUG_CLIPBOARD
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
nsClipboard::SelectionReceiver (GtkWidget *aWidget,
                                GtkSelectionData *aSD)
{
  mBlocking = PR_FALSE;

  if (aSD->length < 0)
  {
#ifdef DEBUG_CLIPBOARD
    g_print("        Error retrieving selection: length was %d\n", aSD->length);
#endif
    return;
  }

  nsCAutoString type(gdk_atom_name(aSD->type));

  if (type.Equals("UTF8_STRING")) {
    mSelectionData = *aSD;

    static const PRUnichar unicodeFormatter[] = {
      (PRUnichar)'%',
      (PRUnichar)'s',
      (PRUnichar)0,
    };
    // convert aSD->data (UTF8) to Unicode and place the unicode data in mSelectionData.data
    PRUnichar *unicodeString = nsTextFormatter::smprintf(unicodeFormatter, aSD->data);

    if (!unicodeString) // this would be bad wouldn't it?
      return;

    int len = sizeof(unicodeString);
    mSelectionData.data = g_new(guchar, len + 2);
    memcpy(mSelectionData.data,
           unicodeString,
           len);
    nsTextFormatter::smprintf_free(unicodeString);
    mSelectionData.type = gdk_atom_intern(kUnicodeMime, FALSE);
    mSelectionData.length = len;

  } else if (type.Equals("STRING") || type.Equals(kTextMime)) {
#ifdef DEBUG_CLIPBOARD
    g_print("        Copying mSelectionData pointer -- \n");
#endif
    mSelectionData = *aSD;
    mSelectionData.data = NS_REINTERPRET_CAST(guchar*, nsAllocator::Alloc(aSD->length + 1));
#ifdef DEBUG_CLIPBOARD
    g_print("         Data = %s\n         Length = %i\n", aSD->data, aSD->length);
#endif
    memcpy(mSelectionData.data,
           aSD->data,
           aSD->length);
    // Null terminate in case anyone cares,
    // and so we can print the string for debugging:
    mSelectionData.data[aSD->length] = '\0';
    mSelectionData.length = aSD->length;

  } else {
    mSelectionData = *aSD;
    mSelectionData.data = g_new(guchar, aSD->length + 1);
    memcpy(mSelectionData.data,
           aSD->data,
           aSD->length);
    mSelectionData.length = aSD->length;
  }
}



/**
 * Some platforms support deferred notification for putting data on the clipboard
 * This method forces the data onto the clipboard in its various formats
 * This may be used if the application going away.
 *
 * @result NS_OK if successful.
 */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  SetWhichClipboard(aWhichClipboard);
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::ForceDataToClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(nsISupportsArray* aFlavorList, 
                                    PRInt32 aWhichClipboard, 
                                    PRBool * outResult)
{
  // XXX this doesn't work right.  need to fix it.
  
  // Note to implementor...(from pink the clipboard bitch).
  //
  // If a client asks for unicode, first check if unicode is present. If not, then 
  // check for plain text. If it's there, say "yes" as we will do the conversion
  // in GetNativeClipboardData(). From this point on, no client will
  // ever ask for text/plain explicitly. If they do, you must ASSERT!
  SetWhichClipboard(aWhichClipboard);

#if 0
  *outResult = PR_FALSE;
  PRUint32 length;
  aFlavorList->Count(&length);
  for ( PRUint32 i = 0; i < length; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> flavorWrapper ( do_QueryInterface(genericFlavor) );
    if ( flavorWrapper ) {
      nsXPIDLCString flavor;
      flavorWrapper->ToString ( getter_Copies(flavor) );
      
      gint format = GetFormat(flavor);
      if (DoConvert(format)) {
        *outResult = PR_TRUE;
        break;
      }
    }
  }
#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::HasDataMatchingFlavors() called -- returning %i\n", *outResult);
#endif

#else
  *outResult = PR_TRUE;
#endif

  return NS_OK;

}


/**
 * This is the callback which is called when another app
 * requests the selection.
 *
 * @param  widget The widget
 * @param  aSelectionData Selection data
 * @param  info Value passed in from the callback init
 * @param  time Time when the selection request came in
 */
void nsClipboard::SelectionGetCB(GtkWidget        *widget,
                                 GtkSelectionData *aSelectionData,
                                 guint            aInfo,
                                 guint            aTime)
{
#ifdef DEBUG_CLIPBOARD
  g_print("nsClipboard::SelectionGetCB\n"); 
#endif
  nsClipboard *cb = (nsClipboard *)gtk_object_get_data(GTK_OBJECT(widget),
                                                       "cb");

  void     *clipboardData;
  PRUint32 dataLength;
  nsresult rv;

  // Make sure we have a transferable:
  if (!cb->mTransferable) {
    g_print("Clipboard has no transferable!\n");
    return;
  }
#ifdef DEBUG_CLIPBOARD
  g_print("  aInfo == %d -", aInfo);
#endif
  char* dataFlavor = nsnull;
  nsCAutoString type(gdk_atom_name(aInfo));

  PRBool needToDoConversionToPlainText = PR_FALSE;

  if (type.Equals("STRING")) {
    // if someone was asking for text/plain, lookup unicode instead so we can convert it.
    dataFlavor = kUnicodeMime;
    needToDoConversionToPlainText = PR_TRUE;
  } else if (type.Equals("UTF8_STRING")) {
    // XXX fixme -- need to do conversion here
    dataFlavor = kUnicodeMime;
  } else {
    dataFlavor = type;
  }
#ifdef DEBUG_CLIPBOARD
  g_print("- aInfo is for %s\n", gdk_atom_name(aInfo));
#endif
  // Get data out of transferable.
  nsCOMPtr<nsISupports> genericDataWrapper;
  rv = cb->mTransferable->GetTransferData(dataFlavor,
                                          getter_AddRefs(genericDataWrapper),
                                          &dataLength);
  nsPrimitiveHelpers::CreateDataFromPrimitive ( dataFlavor, genericDataWrapper, &clipboardData, dataLength );
  if (NS_SUCCEEDED(rv) && clipboardData && dataLength > 0) {
    size_t size = 1;
    // find the number of bytes in the data for the below thing
    //    size_t size = sizeof((void*)((unsigned char)clipboardData[0]));
    //    g_print("************  *****************      ******************* %i\n", size);
    
    // if required, do the extra work to convert unicode to plain text and replace the output
    // values with the plain text.
    if ( needToDoConversionToPlainText ) {
      char* plainTextData = nsnull;
      PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, clipboardData);
      PRInt32 plainTextLen = 0;
      nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, dataLength / 2, &plainTextData, &plainTextLen );
      if ( clipboardData ) {
        nsAllocator::Free(NS_REINTERPRET_CAST(char*, clipboardData));
        clipboardData = plainTextData;
        dataLength = plainTextLen;
      }
    }

    if ( clipboardData )
      gtk_selection_data_set(aSelectionData,
                             aInfo, size*8,
                             NS_REINTERPRET_CAST(unsigned char *, clipboardData),
                             dataLength);
    nsCRT::free ( NS_REINTERPRET_CAST(char*, clipboardData) );
  }
  else
    printf("Transferable didn't support data flavor %s (type = %d)\n",
           dataFlavor ? dataFlavor : "None", aInfo);
}


/**
 * Called when another app requests selection ownership
 *
 * @param  aWidget the widget
 * @param  aEvent the GdkEvent for the selection
 * @param  aData value passed in from the callback init
 */
void nsClipboard::SelectionClearCB(GtkWidget *aWidget,
                                   GdkEventSelection *aEvent,
                                   gpointer aData)
{
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::SelectionClearCB\n");
#endif /* DEBUG_CLIPBOARD */

  nsClipboard *cb = (nsClipboard *)gtk_object_get_data(GTK_OBJECT(aWidget),
                                                       "cb");

  if (aEvent->selection == GDK_SELECTION_PRIMARY) {
    g_print("clearing PRIMARY clipboard\n");
    cb->EmptyClipboard(kSelectionClipboard);
  } else if (aEvent->selection == GDK_SELECTION_CLIPBOARD) {
    g_print("clearing CLIPBOARD clipboard\n");
    cb->EmptyClipboard(kGlobalClipboard);
  }
}


/**
 * The routine called when another app asks for the content of the selection
 *
 * @param  aWidget the widget
 * @param  aSelectionData gtk selection stuff
 * @param  aData value passed in from the callback init
 */
void
nsClipboard::SelectionRequestCB (GtkWidget *aWidget,
                                 GtkSelectionData *aSelectionData,
                                 gpointer aData)
{
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::SelectionRequestCB\n");
#endif /* DEBUG_CLIPBOARD */
}

/**
 * ...
 *
 * @param  aWidget the widget
 * @param  aSelectionData gtk selection stuff
 * @param  aData value passed in from the callback init
 */
void
nsClipboard::SelectionNotifyCB (GtkWidget *aWidget,
                                GtkSelectionData *aSelectionData,
                                gpointer aData)
{
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::SelectionNotifyCB\n");
#endif /* DEBUG_CLIPBOARD */
}



/* helper functions*/

/* inline */
void nsClipboard::SetWhichClipboard(PRInt32 aWhichClipboard)
{
  switch (aWhichClipboard)
  {
  case kGlobalClipboard:
    mSelectionAtom = GDK_SELECTION_CLIPBOARD;
    break;
  case kSelectionClipboard:
    mSelectionAtom = GDK_SELECTION_PRIMARY;
    break;
  }
}

/* inline */
void nsClipboard::AddTarget(GdkAtom aAtom, GdkAtom aSelectionAtom)
{
  gtk_selection_add_target(sWidget, aSelectionAtom, aAtom, aAtom);
}

void nsClipboard::RegisterFormat(const char *aMimeStr, GdkAtom aSelectionAtom)
{
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::RegisterFormat(%s)\n", aMimeStr);
#endif
  nsCAutoString mimeStr(aMimeStr);

  GdkAtom atom = gdk_atom_intern(aMimeStr, FALSE);

  // for Text and Unicode we want to add some extra types to the X clipboard
  if (mimeStr.Equals(kTextMime)) {
    AddTarget(GDK_SELECTION_TYPE_STRING, aSelectionAtom);    // STRING -- we will convert to unicode internally
  } else if (mimeStr.Equals(kUnicodeMime)) {
    AddTarget(gdk_atom_intern("UTF8_STRING", FALSE), aSelectionAtom);
    AddTarget(GDK_SELECTION_TYPE_STRING, aSelectionAtom);    // STRING -- we will convert to unicode internally
  }

  AddTarget(atom, aSelectionAtom);
}

/* return PR_TRUE if we have converted or PR_FALSE if we havn't and need to keep being called */
PRBool nsClipboard::DoConvert(const char *aMimeStr)
{
#ifdef DEBUG_CLIPBOARD
  g_print("  nsClipboard::DoConvert(%s)\n", aMimeStr);
#endif
  /* when doing the selection_add_target, each case should have the same last parameter
     which matches the case match */
  PRBool r = PR_FALSE;

  nsCAutoString mimeStr(aMimeStr);

  if (mimeStr.Equals(kTextMime)) {
    r = DoRealConvert(GDK_SELECTION_TYPE_STRING);
    if (r) return r;
  } else if (mimeStr.Equals(kUnicodeMime)) {
    r = DoRealConvert(gdk_atom_intern("UTF_STRING", FALSE));
    if (r) return r;
  }

  GdkAtom atom = gdk_atom_intern(aMimeStr, FALSE);
  r = DoRealConvert(atom);
  if (r) return r;

  return r;
}
