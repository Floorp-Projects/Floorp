/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard <blizzard@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Markus G. Kuhn <mkuhn@acm.org>
 *   Richard Verhoeven <river@win.tue.nl>
 *   Frank Tang <ftang@netscape.com> adopt into mozilla
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDragService.h"
#include "nsIObserverService.h"
#include "nsWidgetsCID.h"
#include "nsWindow.h"
#include "nsIServiceManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "prlog.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "prtime.h"
#include "prthread.h"
#include <gtk/gtkinvisible.h>
#include <gdk/gdkx.h>
#include "nsCRT.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxASurface.h"
#include "nsImageToPixbuf.h"
#endif

static PRLogModuleInfo *sDragLm = NULL;

static const char gMimeListType[] = "application/x-moz-internal-item-list";
static const char gMozUrlType[] = "_NETSCAPE_URL";
static const char gTextUriListType[] = "text/uri-list";

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE4(nsDragService,
                         nsIDragService,
                         nsIDragSession,
                         nsIDragSessionGTK,
                         nsIObserver)

static void
invisibleSourceDragEnd(GtkWidget        *aWidget,
                       GdkDragContext   *aContext,
                       gpointer          aData);

static void
invisibleSourceDragDataGet(GtkWidget        *aWidget,
                           GdkDragContext   *aContext,
                           GtkSelectionData *aSelectionData,
                           guint             aInfo,
                           guint32           aTime,
                           gpointer          aData);

nsDragService::nsDragService()
{
    // We have to destroy the hidden widget before the event loop stops
    // running.
    nsCOMPtr<nsIObserverService> obsServ =
        do_GetService("@mozilla.org/observer-service;1");
    obsServ->AddObserver(this, "quit-application", PR_FALSE);

    // our hidden source widget
    mHiddenWidget = gtk_invisible_new();
    // make sure that the widget is realized so that
    // we can use it as a drag source.
    gtk_widget_realize(mHiddenWidget);
    // hook up our internal signals so that we can get some feedback
    // from our drag source
    gtk_signal_connect(GTK_OBJECT(mHiddenWidget), "drag_data_get",
                       GTK_SIGNAL_FUNC(invisibleSourceDragDataGet), this);
    gtk_signal_connect(GTK_OBJECT(mHiddenWidget), "drag_end",
                       GTK_SIGNAL_FUNC(invisibleSourceDragEnd), this);

    // set up our logging module
    if (!sDragLm)
    sDragLm = PR_NewLogModule("nsDragService");
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::nsDragService"));
    mTargetWidget = 0;
    mTargetDragContext = 0;
    mTargetTime = 0;
    mCanDrop = PR_FALSE;
    mTargetDragDataReceived = PR_FALSE;
    mTargetDragData = 0;
    mTargetDragDataLen = 0;
}

nsDragService::~nsDragService()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::~nsDragService"));
}

// nsIObserver

NS_IMETHODIMP
nsDragService::Observe(nsISupports *aSubject, const char *aTopic,
                       const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "quit-application")) {
    PR_LOG(sDragLm, PR_LOG_DEBUG,
           ("nsDragService::Observe(\"quit-application\")"));
    if (mHiddenWidget) {
      gtk_widget_destroy(mHiddenWidget);
      mHiddenWidget = 0;
    }
    TargetResetData();
  } else {
    NS_NOTREACHED("unexpected topic");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

// nsIDragService

NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                 nsISupportsArray * aArrayTransferables,
                                 nsIScriptableRegion * aRegion,
                                 PRUint32 aActionType)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::InvokeDragSession"));
    nsBaseDragService::InvokeDragSession(aDOMNode, aArrayTransferables,
                                         aRegion, aActionType);
    // make sure that we have an array of transferables to use
    if (!aArrayTransferables)
        return NS_ERROR_INVALID_ARG;
    // set our reference to the transferables.  this will also addref
    // the transferables since we're going to hang onto this beyond the
    // length of this call
    mSourceDataItems = aArrayTransferables;
    // get the list of items we offer for drags
    GtkTargetList *sourceList = 0;

    sourceList = GetSourceList();

    if (sourceList) {
        // save our action type
        GdkDragAction action = GDK_ACTION_DEFAULT;

        if (aActionType & DRAGDROP_ACTION_COPY)
            action = (GdkDragAction)(action | GDK_ACTION_COPY);
        if (aActionType & DRAGDROP_ACTION_MOVE)
            action = (GdkDragAction)(action | GDK_ACTION_MOVE);
        if (aActionType & DRAGDROP_ACTION_LINK)
            action = (GdkDragAction)(action | GDK_ACTION_LINK);

        // Create a fake event for the drag so we can pass the time
        // (so to speak.)  If we don't do this the drag can end as a
        // result of a button release that is actually _earlier_ than
        // CurrentTime.  So we use the time on the last button press
        // event, as that will always be older than the button release
        // that ends any drag.
        GdkEvent event;
        memset(&event, 0, sizeof(GdkEvent));
        event.type = GDK_BUTTON_PRESS;
        event.button.window = mHiddenWidget->window;
        event.button.time = nsWindow::mLastButtonPressTime;

        // start our drag.
        GdkDragContext *context = gtk_drag_begin(mHiddenWidget,
                                                 sourceList,
                                                 action,
                                                 1,
                                                 &event);

#ifdef MOZ_CAIRO_GFX
        GdkPixbuf* dragPixbuf = nsnull;
        nsRect dragRect;
        if (mHasImage || mSelection) {
          nsRefPtr<gfxASurface> surface;
          DrawDrag(aDOMNode, aRegion, mScreenX, mScreenY,
                   &dragRect, getter_AddRefs(surface));
          if (surface) {
            dragPixbuf =
              nsImageToPixbuf::SurfaceToPixbuf(surface, dragRect.width, dragRect.height);
          }
        }

        if (dragPixbuf)
          gtk_drag_set_icon_pixbuf(context, dragPixbuf,
                                   mScreenX - NSToIntRound(dragRect.x),
                                   mScreenY - NSToIntRound(dragRect.y));
        else
          gtk_drag_set_icon_default(context);
#else
        // use a default icon
        gtk_drag_set_icon_default(context);
#endif
        gtk_target_list_unref(sourceList);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDragService::StartDragSession()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::StartDragSession"));
    return nsBaseDragService::StartDragSession();
}
 
NS_IMETHODIMP
nsDragService::EndDragSession()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::EndDragSession"));
    // unset our drag action
    SetDragAction(DRAGDROP_ACTION_NONE);
    return nsBaseDragService::EndDragSession();
}

// nsIDragSession
NS_IMETHODIMP
nsDragService::SetCanDrop(PRBool aCanDrop)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::SetCanDrop %d",
                                   aCanDrop));
    mCanDrop = aCanDrop;
    return NS_OK;
}

NS_IMETHODIMP
nsDragService::GetCanDrop(PRBool *aCanDrop)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetCanDrop"));
    *aCanDrop = mCanDrop;
    return NS_OK;
}

// count the number of URIs in some text/uri-list format data.
static PRUint32
CountTextUriListItems(const char *data,
                      PRUint32 datalen)
{
    const char *p = data;
    const char *endPtr = p + datalen;
    PRUint32 count = 0;

    while (p < endPtr) {
        // skip whitespace (if any)
        while (p < endPtr && *p != '\0' && isspace(*p))
            p++;
        // if we aren't at the end of the line ...
        if (p != endPtr && *p != '\0' && *p != '\n' && *p != '\r')
            count++;
        // skip to the end of the line
        while (p < endPtr && *p != '\0' && *p != '\n')
            p++;
        p++; // skip the actual newline as well.
    }
    return count;
}

// extract an item from text/uri-list formatted data and convert it to
// unicode.
static void
GetTextUriListItem(const char *data,
                   PRUint32 datalen,
                   PRUint32 aItemIndex,
                   PRUnichar **convertedText,
                   PRInt32 *convertedTextLen)
{
    const char *p = data;
    const char *endPtr = p + datalen;
    unsigned int count = 0;

    *convertedText = nsnull;
    while (p < endPtr) {
        // skip whitespace (if any)
        while (p < endPtr && *p != '\0' && isspace(*p))
            p++;
        // if we aren't at the end of the line, we have a url
        if (p != endPtr && *p != '\0' && *p != '\n' && *p != '\r')
            count++;
        // this is the item we are after ...
        if (aItemIndex + 1 == count) {
            const char *q = p;
            while (q < endPtr && *q != '\0' && *q != '\n' && *q != '\r')
              q++;
            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(
                                p, q - p, convertedText, convertedTextLen);
            break;
        }
        // skip to the end of the line
        while (p < endPtr && *p != '\0' && *p != '\n')
            p++;
        p++; // skip the actual newline as well.
    }

    // didn't find the desired item, so just pass the whole lot
    if (!*convertedText) {
        nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(
                            data, datalen, convertedText, convertedTextLen);
    }
}

NS_IMETHODIMP
nsDragService::GetNumDropItems(PRUint32 * aNumItems)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetNumDropItems"));
    PRBool isList = IsTargetContextList();
    if (isList)
        mSourceDataItems->Count(aNumItems);
    else {
        GdkAtom gdkFlavor = gdk_atom_intern(gTextUriListType, FALSE);
        GetTargetDragData(gdkFlavor);
        if (mTargetDragData) {
            const char *data = NS_REINTERPRET_CAST(char*, mTargetDragData);
            *aNumItems = CountTextUriListItems(data, mTargetDragDataLen);
        } else
            *aNumItems = 1;
    }
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("%d items", *aNumItems));
    return NS_OK;
}


NS_IMETHODIMP
nsDragService::GetData(nsITransferable * aTransferable,
                       PRUint32 aItemIndex)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetData %d", aItemIndex));

    // make sure that we have a transferable
    if (!aTransferable)
        return NS_ERROR_INVALID_ARG;

    // get flavor list that includes all acceptable flavors (including
    // ones obtained through conversion). Flavors are nsISupportsStrings
    // so that they can be seen from JS.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsISupportsArray> flavorList;
    rv = aTransferable->FlavorsTransferableCanImport(
                        getter_AddRefs(flavorList));
    if (NS_FAILED(rv))
        return rv;

    // count the number of flavors
    PRUint32 cnt;
    flavorList->Count(&cnt);
    unsigned int i;

    // check to see if this is an internal list
    PRBool isList = IsTargetContextList();

    if (isList) {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("it's a list..."));
        nsCOMPtr<nsISupports> genericWrapper;
        // there is always one element if it's a list
        flavorList->GetElementAt(0, getter_AddRefs(genericWrapper));
        nsCOMPtr<nsISupportsCString> currentFlavor;
        currentFlavor = do_QueryInterface(genericWrapper);
        if (currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString(getter_Copies(flavorStr));
            PR_LOG(sDragLm,
                   PR_LOG_DEBUG,
                   ("flavor is %s\n", (const char *)flavorStr));
            // get the item with the right index
            nsCOMPtr<nsISupports> genericItem;
            mSourceDataItems->GetElementAt(aItemIndex,
                                           getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> item(do_QueryInterface(genericItem));
            if (item) {
                nsCOMPtr<nsISupports> data;
                PRUint32 tmpDataLen = 0;
                PR_LOG(sDragLm, PR_LOG_DEBUG,
                       ("trying to get transfer data for %s\n",
                       (const char *)flavorStr));
                rv = item->GetTransferData(flavorStr,
                                           getter_AddRefs(data),
                                           &tmpDataLen);
                if (NS_FAILED(rv)) {
                    PR_LOG(sDragLm, PR_LOG_DEBUG, ("failed.\n"));
                    return NS_ERROR_FAILURE;
                }
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("succeeded.\n"));
                rv = aTransferable->SetTransferData(flavorStr,data,tmpDataLen);
                if (NS_FAILED(rv)) {
                    PR_LOG(sDragLm,
                           PR_LOG_DEBUG,
                           ("fail to set transfer data into transferable!\n"));
                    return NS_ERROR_FAILURE;
                }
                // ok, we got the data
                return NS_OK;
            }
        }
        // if we got this far, we failed
        return NS_ERROR_FAILURE;
    }

    // Now walk down the list of flavors. When we find one that is
    // actually present, copy out the data into the transferable in that
    // format. SetTransferData() implicitly handles conversions.
    for ( i = 0; i < cnt; ++i ) {
        nsCOMPtr<nsISupports> genericWrapper;
        flavorList->GetElementAt(i,getter_AddRefs(genericWrapper));
        nsCOMPtr<nsISupportsCString> currentFlavor;
        currentFlavor = do_QueryInterface(genericWrapper);
        if (currentFlavor) {
            // find our gtk flavor
            nsXPIDLCString flavorStr;
            currentFlavor->ToString(getter_Copies(flavorStr));
            GdkAtom gdkFlavor = gdk_atom_intern(flavorStr, FALSE);
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("looking for data in type %s, gdk flavor %ld\n",
                   NS_STATIC_CAST(const char*,flavorStr), gdkFlavor));
            PRBool dataFound = PR_FALSE;
            if (gdkFlavor) {
                GetTargetDragData(gdkFlavor);
            }
            if (mTargetDragData) {
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("dataFound = PR_TRUE\n"));
                dataFound = PR_TRUE;
            }
            else {
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("dataFound = PR_FALSE\n"));
                // if we are looking for text/unicode and we fail to find it
                // on the clipboard first, try again with text/plain. If that
                // is present, convert it to unicode.
                if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
                    PR_LOG(sDragLm, PR_LOG_DEBUG,
                           ("we were looking for text/unicode... \
                           trying again with text/plain\n"));
                    gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        PR_LOG(sDragLm, PR_LOG_DEBUG, ("Got textplain data\n"));
                        const char* castedText =
                                    NS_REINTERPRET_CAST(char*, mTargetDragData);
                        PRUnichar* convertedText = nsnull;
                        PRInt32 convertedTextLen = 0;
                        nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(
                                            castedText, mTargetDragDataLen,
                                            &convertedText, &convertedTextLen);
                        if ( convertedText ) {
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("successfully converted plain text \
                                   to unicode.\n"));
                            // out with the old, in with the new
                            g_free(mTargetDragData);
                            mTargetDragData = convertedText;
                            mTargetDragDataLen = convertedTextLen * 2;
                            dataFound = PR_TRUE;
                        } // if plain text data on clipboard
                    } // if plain text flavor present
                } // if looking for text/unicode

                // if we are looking for text/x-moz-url and we failed to find
                // it on the clipboard, try again with text/uri-list, and then
                // _NETSCAPE_URL
                if (strcmp(flavorStr, kURLMime) == 0) {
                    PR_LOG(sDragLm, PR_LOG_DEBUG,
                           ("we were looking for text/x-moz-url...\
                           trying again with text/uri-list\n"));
                    gdkFlavor = gdk_atom_intern(gTextUriListType, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        PR_LOG(sDragLm, PR_LOG_DEBUG,
                               ("Got text/uri-list data\n"));
                        const char *data =
                                   NS_REINTERPRET_CAST(char*, mTargetDragData);
                        PRUnichar* convertedText = nsnull;
                        PRInt32 convertedTextLen = 0;

                        GetTextUriListItem(data, mTargetDragDataLen, aItemIndex,
                                           &convertedText, &convertedTextLen);

                        if ( convertedText ) {
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("successfully converted \
                                   _NETSCAPE_URL to unicode.\n"));
                            // out with the old, in with the new
                            g_free(mTargetDragData);
                            mTargetDragData = convertedText;
                            mTargetDragDataLen = convertedTextLen * 2;
                            dataFound = PR_TRUE;
                        }
                    }
                    else {
                        PR_LOG(sDragLm, PR_LOG_DEBUG,
                               ("failed to get text/uri-list data\n"));
                    }
                    if (!dataFound) {
                        PR_LOG(sDragLm, PR_LOG_DEBUG,
                               ("we were looking for text/x-moz-url...\
                               trying again with _NETSCAP_URL\n"));
                        gdkFlavor = gdk_atom_intern(gMozUrlType, FALSE);
                        GetTargetDragData(gdkFlavor);
                        if (mTargetDragData) {
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("Got _NETSCAPE_URL data\n"));
                            const char* castedText =
                                  NS_REINTERPRET_CAST(char*, mTargetDragData);
                            PRUnichar* convertedText = nsnull;
                            PRInt32 convertedTextLen = 0;
                            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(castedText, mTargetDragDataLen, &convertedText, &convertedTextLen);
                            if ( convertedText ) {
                                PR_LOG(sDragLm,
                                       PR_LOG_DEBUG,
                                       ("successfully converted _NETSCAPE_URL \
                                       to unicode.\n"));
                                // out with the old, in with the new
                                g_free(mTargetDragData);
                                mTargetDragData = convertedText;
                                mTargetDragDataLen = convertedTextLen * 2;
                                dataFound = PR_TRUE;
                            }
                        }
                        else {
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("failed to get _NETSCAPE_URL data\n"));
                        }
                    }
                }

            } // else we try one last ditch effort to find our data

            if (dataFound) {
                // the DOM only wants LF, so convert from MacOS line endings
                // to DOM line endings.
                nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(
                             flavorStr,
                             &mTargetDragData,
                             NS_REINTERPRET_CAST(int*, &mTargetDragDataLen));
        
                // put it into the transferable.
                nsCOMPtr<nsISupports> genericDataWrapper;
                nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr,
                                    mTargetDragData, mTargetDragDataLen,
                                    getter_AddRefs(genericDataWrapper));
                aTransferable->SetTransferData(flavorStr,
                                               genericDataWrapper,
                                               mTargetDragDataLen);
                // we found one, get out of this loop!
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("dataFound and converted!\n"));
                break;
            }
        } // if (currentFlavor)
    } // foreach flavor

    return NS_OK;
  
}

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor,
                                     PRBool *_retval)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::IsDataFlavorSupported %s",
                                   aDataFlavor));
    if (!_retval)
        return NS_ERROR_INVALID_ARG;

    // set this to no by default
    *_retval = PR_FALSE;

    // check to make sure that we have a drag object set, here
    if (!mTargetDragContext) {
        PR_LOG(sDragLm, PR_LOG_DEBUG,
               ("*** warning: IsDataFlavorSupported \
               called without a valid drag context!\n"));
        return NS_OK;
    }

    // check to see if the target context is a list.
    PRBool isList = IsTargetContextList();
    // if it is, just look in the internal data since we are the source
    // for it.
    if (isList) {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("It's a list.."));
        PRUint32 numDragItems = 0;
        // if we don't have mDataItems we didn't start this drag so it's
        // an external client trying to fool us.
        if (!mSourceDataItems)
            return NS_OK;
        mSourceDataItems->Count(&numDragItems);
        for (PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
            nsCOMPtr<nsISupports> genericItem;
            mSourceDataItems->GetElementAt(itemIndex,
                                           getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> currItem(do_QueryInterface(genericItem));
            if (currItem) {
                nsCOMPtr <nsISupportsArray> flavorList;
                currItem->FlavorsTransferableCanExport(
                          getter_AddRefs(flavorList));
                if (flavorList) {
                    PRUint32 numFlavors;
                    flavorList->Count( &numFlavors );
                    for ( PRUint32 flavorIndex = 0;
                          flavorIndex < numFlavors ;
                          ++flavorIndex ) {
                        nsCOMPtr<nsISupports> genericWrapper;
                        flavorList->GetElementAt(flavorIndex,
                                                getter_AddRefs(genericWrapper));
                        nsCOMPtr<nsISupportsCString> currentFlavor;
                        currentFlavor = do_QueryInterface(genericWrapper);
                        if (currentFlavor) {
                            nsXPIDLCString flavorStr;
                            currentFlavor->ToString(getter_Copies(flavorStr));
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("checking %s against %s\n",
                                   (const char *)flavorStr, aDataFlavor));
                            if (strcmp(flavorStr, aDataFlavor) == 0) {
                                PR_LOG(sDragLm, PR_LOG_DEBUG,
                                       ("boioioioiooioioioing!\n"));
                                *_retval = PR_TRUE;
                            }
                        }
                    }
                }
            }
        }
        return NS_OK;
    }

    // check the target context vs. this flavor, one at a time
    GList *tmp;
    for (tmp = mTargetDragContext->targets; tmp; tmp = tmp->next) {
        GdkAtom atom = (GdkAtom)GPOINTER_TO_INT(tmp->data);
        gchar *name = NULL;
        name = gdk_atom_name(atom);
        PR_LOG(sDragLm, PR_LOG_DEBUG,
               ("checking %s against %s\n", name, aDataFlavor));
        if (name && (strcmp(name, aDataFlavor) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("good!\n"));
            *_retval = PR_TRUE;
        }
        // check for automatic text/uri-list -> text/x-moz-url mapping
        if (*_retval == PR_FALSE && 
            name &&
            (strcmp(name, gTextUriListType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's text/uri-list and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = PR_TRUE;
        }
        // check for automatic _NETSCAPE_URL -> text/x-moz-url mapping
        if (*_retval == PR_FALSE && 
            name &&
            (strcmp(name, gMozUrlType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's _NETSCAPE_URL and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = PR_TRUE;
        }
        // check for auto text/plain -> text/unicode mapping
        if (*_retval == PR_FALSE && 
            name &&
            (strcmp(name, kTextMime) == 0) &&
            (strcmp(aDataFlavor, kUnicodeMime) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's text plain and we're checking \
                   against text/unicode )\n"));
            *_retval = PR_TRUE;
        }
        g_free(name);
    }
    return NS_OK;
}

// nsIDragSessionGTK

NS_IMETHODIMP
nsDragService::TargetSetLastContext(GtkWidget      *aWidget,
                                    GdkDragContext *aContext,
                                    guint           aTime)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::TargetSetLastContext"));
    mTargetWidget = aWidget;
    mTargetDragContext = aContext;
    mTargetTime = aTime;
    return NS_OK;
}

NS_IMETHODIMP
nsDragService::TargetStartDragMotion(void)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::TargetStartDragMotion"));
    mCanDrop = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsDragService::TargetEndDragMotion(GtkWidget      *aWidget,
                                   GdkDragContext *aContext,
                                   guint           aTime)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG,
           ("nsDragService::TargetEndDragMotion %d", mCanDrop));

    if (mCanDrop) {
        GdkDragAction action;
        // notify the dragger if we can drop
        switch (mDragAction) {
        case DRAGDROP_ACTION_COPY:
          action = GDK_ACTION_COPY;
          break;
        case DRAGDROP_ACTION_LINK:
          action = GDK_ACTION_LINK;
          break;
        default:
          action = GDK_ACTION_MOVE;
          break;
        }
        gdk_drag_status(aContext, action, aTime);
    }
    else {
        gdk_drag_status(aContext, (GdkDragAction)0, aTime);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDragService::TargetDataReceived(GtkWidget         *aWidget,
                                  GdkDragContext    *aContext,
                                  gint               aX,
                                  gint               aY,
                                  GtkSelectionData  *aSelectionData,
                                  guint              aInfo,
                                  guint32            aTime)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::TargetDataReceived"));
    TargetResetData();
    mTargetDragDataReceived = PR_TRUE;
    if (aSelectionData->length > 0) {
        mTargetDragDataLen = aSelectionData->length;
        mTargetDragData = g_malloc(mTargetDragDataLen);
        memcpy(mTargetDragData, aSelectionData->data, mTargetDragDataLen);
    }
    else {
        PR_LOG(sDragLm, PR_LOG_DEBUG,
               ("Failed to get data.  selection data len was %d\n",
                aSelectionData->length));
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDragService::TargetSetTimeCallback(nsIDragSessionGTKTimeCB aCallback)
{
    return NS_OK;
}


PRBool
nsDragService::IsTargetContextList(void)
{
    PRBool retval = PR_FALSE;

    if (!mTargetDragContext)
        return retval;

    // gMimeListType drags only work for drags within a single process.
    // The gtk_drag_get_source_widget() function will return NULL if the
    // source of the drag is another app, so we use it to check if a
    // gMimeListType drop will work or not.
    if (gtk_drag_get_source_widget(mTargetDragContext) == NULL)
        return retval;

    GList *tmp;

    // walk the list of context targets and see if one of them is a list
    // of items.
    for (tmp = mTargetDragContext->targets; tmp; tmp = tmp->next) {
        GdkAtom atom = (GdkAtom)GPOINTER_TO_INT(tmp->data);
        gchar *name = NULL;
        name = gdk_atom_name(atom);
        if (strcmp(name, gMimeListType) == 0)
            retval = PR_TRUE;
        g_free(name);
        if (retval)
            break;
    }
    return retval;
}

// Maximum time to wait for a "drag_received" arrived, in microseconds
#define NS_DND_TIMEOUT 500000

void
nsDragService::GetTargetDragData(GdkAtom aFlavor)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("getting data flavor %d\n", aFlavor));
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("mLastWidget is %p and mLastContext is %p\n",
                                   mTargetWidget, mTargetDragContext));
    // reset our target data areas
    TargetResetData();
    gtk_drag_get_data(mTargetWidget, mTargetDragContext, aFlavor, mTargetTime);
    
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("about to start inner iteration."));
    PRTime entryTime = PR_Now();
    while (!mTargetDragDataReceived && mDoingDrag) {
        // check the number of iterations
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("doing iteration...\n"));
        PR_Sleep(20*PR_TicksPerSecond()/1000);  /* sleep for 20 ms/iteration */
        if (PR_Now()-entryTime > NS_DND_TIMEOUT) break;
        gtk_main_iteration();
    }
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("finished inner iteration\n"));
}

void
nsDragService::TargetResetData(void)
{
    mTargetDragDataReceived = PR_FALSE;
    // make sure to free old data if we have to
    if (mTargetDragData)
      g_free(mTargetDragData);
    mTargetDragData = 0;
    mTargetDragDataLen = 0;
}

GtkTargetList *
nsDragService::GetSourceList(void)
{
    if (!mSourceDataItems)
        return NULL;
    nsVoidArray targetArray;
    GtkTargetEntry *targets;
    GtkTargetList  *targetList = 0;
    PRUint32 targetCount = 0;
    unsigned int numDragItems = 0;

    mSourceDataItems->Count(&numDragItems);

    // Check to see if we're dragging > 1 item.
    if (numDragItems > 1) {
        // as the Xdnd protocol only supports a single item (or is it just
        // gtk's implementation?), we don't advertise all flavours listed
        // in the nsITransferable.

        // the application/x-moz-internal-item-list format, which preserves
        // all information for drags within the same mozilla instance.
        GdkAtom listAtom = gdk_atom_intern(gMimeListType, FALSE);
        GtkTargetEntry *listTarget =
            (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
        listTarget->target = g_strdup(gMimeListType);
        listTarget->flags = 0;
        listTarget->info = GPOINTER_TO_UINT(listAtom);
        PR_LOG(sDragLm, PR_LOG_DEBUG,
               ("automatically adding target %s with id %ld\n",
               listTarget->target, listAtom));
        targetArray.AppendElement(listTarget);

        // check what flavours are supported so we can decide what other
        // targets to advertise.
        nsCOMPtr<nsISupports> genericItem;
        mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> currItem(do_QueryInterface(genericItem));

        if (currItem) {
            nsCOMPtr <nsISupportsArray> flavorList;
            currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
            if (flavorList) {
                PRUint32 numFlavors;
                flavorList->Count( &numFlavors );
                for (PRUint32 flavorIndex = 0;
                     flavorIndex < numFlavors ;
                     ++flavorIndex ) {
                    nsCOMPtr<nsISupports> genericWrapper;
                    flavorList->GetElementAt(flavorIndex,
                                           getter_AddRefs(genericWrapper));
                    nsCOMPtr<nsISupportsCString> currentFlavor;
                    currentFlavor = do_QueryInterface(genericWrapper);
                    if (currentFlavor) {
                        nsXPIDLCString flavorStr;
                        currentFlavor->ToString(getter_Copies(flavorStr));

                        // check if text/x-moz-url is supported.
                        // If so, advertise
                        // text/uri-list.
                        if (strcmp(flavorStr, kURLMime) == 0) {
                            listAtom = gdk_atom_intern(gTextUriListType, FALSE);
                            listTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            listTarget->target = g_strdup(gTextUriListType);
                            listTarget->flags = 0;
                            listTarget->info = GPOINTER_TO_UINT(listAtom);
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("automatically adding target %s with \
                                   id %ld\n", listTarget->target, listAtom));
                            targetArray.AppendElement(listTarget);
                        }
                    }
                } // foreach flavor in item
            } // if valid flavor list
        } // if item is a transferable
    } else if (numDragItems == 1) {
        nsCOMPtr<nsISupports> genericItem;
        mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> currItem(do_QueryInterface(genericItem));
        if (currItem) {
            nsCOMPtr <nsISupportsArray> flavorList;
            currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
            if (flavorList) {
                PRUint32 numFlavors;
                flavorList->Count( &numFlavors );
                for (PRUint32 flavorIndex = 0;
                     flavorIndex < numFlavors ;
                     ++flavorIndex ) {
                    nsCOMPtr<nsISupports> genericWrapper;
                    flavorList->GetElementAt(flavorIndex,
                                             getter_AddRefs(genericWrapper));
                    nsCOMPtr<nsISupportsCString> currentFlavor;
                    currentFlavor = do_QueryInterface(genericWrapper);
                    if (currentFlavor) {
                        nsXPIDLCString flavorStr;
                        currentFlavor->ToString(getter_Copies(flavorStr));
                        // get the atom
                        GdkAtom atom = gdk_atom_intern(flavorStr, FALSE);
                        GtkTargetEntry *target =
                          (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                        target->target = g_strdup(flavorStr);
                        target->flags = 0;
                        target->info = GPOINTER_TO_UINT(atom);
                        PR_LOG(sDragLm, PR_LOG_DEBUG,
                               ("adding target %s with id %ld\n",
                               target->target, atom));
                        targetArray.AppendElement(target);
                        // Check to see if this is text/unicode.
                        // If it is, add text/plain
                        // since we automatically support text/plain
                        // if we support text/unicode.
                        if (strcmp(flavorStr, kUnicodeMime) == 0) {
                            // get the atom for the unicode string
                            GdkAtom plainAtom =
                              gdk_atom_intern(kTextMime, FALSE);
                            GtkTargetEntry *plainTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            plainTarget->target = g_strdup(kTextMime);
                            plainTarget->flags = 0;
                            plainTarget->info = GPOINTER_TO_UINT(plainAtom);
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("automatically adding target %s with \
                                   id %ld\n", plainTarget->target, plainAtom));
                            targetArray.AppendElement(plainTarget);
                        }
                        // Check to see if this is the x-moz-url type.
                        // If it is, add _NETSCAPE_URL
                        // this is a type used by everybody.
                        if (strcmp(flavorStr, kURLMime) == 0) {
                            // get the atom name for it
                            GdkAtom urlAtom =
                             gdk_atom_intern(gMozUrlType, FALSE);
                            GtkTargetEntry *urlTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            urlTarget->target = g_strdup(gMozUrlType);
                            urlTarget->flags = 0;
                            urlTarget->info = GPOINTER_TO_UINT(urlAtom);
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("automatically adding target %s with \
                                   id %ld\n", urlTarget->target, urlAtom));
                            targetArray.AppendElement(urlTarget);
                        }
                    }
                } // foreach flavor in item
            } // if valid flavor list
        } // if item is a transferable
    } // if it is a single item drag

    // get all the elements that we created.
    targetCount = targetArray.Count();
    if (targetCount) {
        // allocate space to create the list of valid targets
        targets =
          (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry) * targetCount);
        PRUint32 targetIndex;
        for ( targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
            GtkTargetEntry *disEntry =
              (GtkTargetEntry *)targetArray.ElementAt(targetIndex);
            // this is a string reference but it will be freed later.
            targets[targetIndex].target = disEntry->target;
            targets[targetIndex].flags = disEntry->flags;
            targets[targetIndex].info = disEntry->info;
        }
        targetList = gtk_target_list_new(targets, targetCount);
        // clean up the target list
        for (PRUint32 cleanIndex = 0; cleanIndex < targetCount; ++cleanIndex) {
            GtkTargetEntry *thisTarget =
              (GtkTargetEntry *)targetArray.ElementAt(cleanIndex);
            g_free(thisTarget->target);
            g_free(thisTarget);
        }
        g_free(targets);
    }
    return targetList;
}

void
nsDragService::SourceEndDrag(void)
{
    // this just releases the list of data items that we provide
    mSourceDataItems = 0;

    // Inform the drag session that we're ending the drag.
    EndDragSession();
}

static void
CreateUriList(nsISupportsArray *items, gchar **text, gint *length)
{
    PRUint32 i, count;
    GString *uriList = g_string_new(NULL);

    items->Count(&count);
    for (i = 0; i < count; i++) {
        nsCOMPtr<nsISupports> genericItem;
        items->GetElementAt(i, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> item;
        item = do_QueryInterface(genericItem);

        if (item) {
            PRUint32 tmpDataLen = 0;
            void    *tmpData = NULL;
            nsresult rv = 0;
            nsCOMPtr<nsISupports> data;
            rv = item->GetTransferData(kURLMime,
                                       getter_AddRefs(data),
                                       &tmpDataLen);

            if (NS_SUCCEEDED(rv)) {
                nsPrimitiveHelpers::CreateDataFromPrimitive(kURLMime,
                                                            data,
                                                            &tmpData,
                                                            tmpDataLen);
                char* plainTextData = nsnull;
                PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*,
                                                               tmpData);
                PRInt32 plainTextLen = 0;
                nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText(
                                    castedUnicode,
                                    tmpDataLen / 2,
                                    &plainTextData,
                                    &plainTextLen);
                if (plainTextData) {
                    PRInt32 j;

                    // text/x-moz-url is of form url + "\n" + title.
                    // We just want the url.
                    for (j = 0; j < plainTextLen; j++)
                        if (plainTextData[j] == '\n' ||
                            plainTextData[j] == '\r') {
                            plainTextData[j] = '\0';
                            break;
                        }
                    g_string_append(uriList, plainTextData);
                    g_string_append(uriList, "\r\n");
                    // this wasn't allocated with glib
                    free(plainTextData);
                }
                if (tmpData) {
                    // this wasn't allocated with glib
                    free(tmpData);
                }
            }
        }
    }
    *text = uriList->str;
    *length = uriList->len + 1;
    g_string_free(uriList, FALSE); // don't free the data
}


void
nsDragService::SourceDataGet(GtkWidget        *aWidget,
                             GdkDragContext   *aContext,
                             GtkSelectionData *aSelectionData,
                             guint             aInfo,
                             guint32           aTime)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::SourceDataGet"));
    GdkAtom atom = (GdkAtom)aInfo;
    nsXPIDLCString mimeFlavor;
    gchar *typeName = 0;
    typeName = gdk_atom_name(atom);
    if (!typeName) {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("failed to get atom name.\n"));
        return;
    }

    PR_LOG(sDragLm, PR_LOG_DEBUG, ("Type is %s\n", typeName));
    // make a copy since |nsXPIDLCString| won't use |g_free|...
    mimeFlavor.Adopt(nsCRT::strdup(typeName));
    g_free(typeName);
    // check to make sure that we have data items to return.
    if (!mSourceDataItems) {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("Failed to get our data items\n"));
        return;
    }

    if (strcmp(mimeFlavor, gTextUriListType) == 0) {
        // fall back for text/uri-list
        gchar *uriList;
        gint length;
        CreateUriList(mSourceDataItems, &uriList, &length);
        gtk_selection_data_set(aSelectionData,
                               aSelectionData->target,
                               8, (guchar *)uriList, length);
        g_free(uriList);
        return;
    }

    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item;
    item = do_QueryInterface(genericItem);
    if (item) {
        // if someone was asking for text/plain, lookup unicode instead so
        // we can convert it.
        PRBool needToDoConversionToPlainText = PR_FALSE;
        const char* actualFlavor = mimeFlavor;
        if (strcmp(mimeFlavor,kTextMime) == 0) {
            actualFlavor = kUnicodeMime;
            needToDoConversionToPlainText = PR_TRUE;
        }
        // if someone was asking for _NETSCAPE_URL we need to convert to
        // plain text but we also need to look for x-moz-url
        else if (strcmp(mimeFlavor, gMozUrlType) == 0) {
            actualFlavor = kURLMime;
            needToDoConversionToPlainText = PR_TRUE;
        }
        else
            actualFlavor = mimeFlavor;

        PRUint32 tmpDataLen = 0;
        void    *tmpData = NULL;
        nsresult rv;
        nsCOMPtr<nsISupports> data;
        rv = item->GetTransferData(actualFlavor,
                                   getter_AddRefs(data),
                                   &tmpDataLen);
        if (NS_SUCCEEDED(rv)) {
            nsPrimitiveHelpers::CreateDataFromPrimitive (actualFlavor, data,
                                                         &tmpData, tmpDataLen);
            // if required, do the extra work to convert unicode to plain
            // text and replace the output values with the plain text.
            if (needToDoConversionToPlainText) {
                char* plainTextData = nsnull;
                PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*,
                                                               tmpData);
                PRInt32 plainTextLen = 0;
                nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText(
                                    castedUnicode,
                                    tmpDataLen / 2,
                                    &plainTextData,
                                    &plainTextLen);
                if (tmpData) {
                    // this was not allocated using glib
                    free(tmpData);
                    tmpData = plainTextData;
                    tmpDataLen = plainTextLen;
                }
            }
            if (tmpData) {
                // this copies the data
                gtk_selection_data_set(aSelectionData,
                                       aSelectionData->target,
                                       8,
                                       (guchar *)tmpData, tmpDataLen);
                // this wasn't allocated with glib
                free(tmpData);
            }
        }
    }
}

/* static */
void
invisibleSourceDragDataGet(GtkWidget        *aWidget,
                           GdkDragContext   *aContext,
                           GtkSelectionData *aSelectionData,
                           guint             aInfo,
                           guint32           aTime,
                           gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleDragDataGet"));
    nsDragService *dragService = (nsDragService *)aData;
    dragService->SourceDataGet(aWidget, aContext,
                               aSelectionData, aInfo, aTime);
}

/* static */
void
invisibleSourceDragEnd(GtkWidget        *aWidget,
                       GdkDragContext   *aContext,
                       gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleDragEnd"));
    nsDragService *dragService = (nsDragService *)aData;
    // The drag has ended.  Release the hostages!
    dragService->SourceEndDrag();
}

