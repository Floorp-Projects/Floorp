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
 *   Ginn Chen <ginn.chen@sun.com>
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
#include "nsIIOService.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "nsTArray.h"
#include "nsPrimitiveHelpers.h"
#include "prtime.h"
#include "prthread.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "nsCRT.h"
#include "mozilla/Services.h"

#include "gfxASurface.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "nsImageToPixbuf.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsIFrame.h"

// This sets how opaque the drag image is
#define DRAG_IMAGE_ALPHA_LEVEL 0.5

// These values are copied from GtkDragResult (rather than using GtkDragResult
// directly) so that this code can be compiled against versions of GTK+ that
// do not have GtkDragResult.
// GtkDragResult is available from GTK+ version 2.12.
enum {
  MOZ_GTK_DRAG_RESULT_SUCCESS,
  MOZ_GTK_DRAG_RESULT_NO_TARGET
};

// Some gobject functions expect functions for gpointer arguments.
// gpointer is void* but C++ doesn't like casting functions to void*.
template<class T> static inline gpointer
FuncToGpointer(T aFunction)
{
    return reinterpret_cast<gpointer>
        (reinterpret_cast<uintptr_t>
         // This cast just provides a warning if T is not a function.
         (reinterpret_cast<void (*)()>(aFunction)));
}

static PRLogModuleInfo *sDragLm = NULL;
static guint sMotionEventTimerID;

static const char gMimeListType[] = "application/x-moz-internal-item-list";
static const char gMozUrlType[] = "_NETSCAPE_URL";
static const char gTextUriListType[] = "text/uri-list";
static const char gTextPlainUTF8Type[] = "text/plain;charset=utf-8";

static void
invisibleSourceDragBegin(GtkWidget        *aWidget,
                         GdkDragContext   *aContext,
                         gpointer          aData);

static void
invisibleSourceDragEnd(GtkWidget        *aWidget,
                       GdkDragContext   *aContext,
                       gpointer          aData);

static gboolean
invisibleSourceDragFailed(GtkWidget        *aWidget,
                          GdkDragContext   *aContext,
                          gint              aResult,
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
        mozilla::services::GetObserverService();
    obsServ->AddObserver(this, "quit-application", PR_FALSE);

    // our hidden source widget
    mHiddenWidget = gtk_invisible_new();
    // make sure that the widget is realized so that
    // we can use it as a drag source.
    gtk_widget_realize(mHiddenWidget);
    // hook up our internal signals so that we can get some feedback
    // from our drag source
    g_signal_connect(mHiddenWidget, "drag_begin",
                     G_CALLBACK(invisibleSourceDragBegin), this);
    g_signal_connect(mHiddenWidget, "drag_data_get",
                     G_CALLBACK(invisibleSourceDragDataGet), this);
    g_signal_connect(mHiddenWidget, "drag_end",
                     G_CALLBACK(invisibleSourceDragEnd), this);
    // drag-failed is available from GTK+ version 2.12
    guint dragFailedID = g_signal_lookup("drag-failed",
                                         G_TYPE_FROM_INSTANCE(mHiddenWidget));
    if (dragFailedID) {
        g_signal_connect_closure_by_id(mHiddenWidget, dragFailedID, 0,
                                       g_cclosure_new(G_CALLBACK(invisibleSourceDragFailed),
                                                      this, NULL),
                                       FALSE);
    }

    // set up our logging module
    if (!sDragLm)
        sDragLm = PR_NewLogModule("nsDragService");
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::nsDragService"));
    mGrabWidget = 0;
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

NS_IMPL_ISUPPORTS_INHERITED2(nsDragService, nsBaseDragService,
                             nsIDragSessionGTK, nsIObserver)

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

// Support for periodic drag events

// http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#drag-and-drop-processing-model
// and the Xdnd protocol both recommend that drag events are sent periodically,
// but GTK does not normally provide this.
//
// Here GTK is periodically stimulated by copies of the most recent mouse
// motion events so as to send drag position messages to the destination when
// appropriate (after it has received a status event from the previous
// message).
//
// (If events were sent only on the destination side then the destination
// would have no message to which it could reply with a drag status.  Without
// sending a drag status to the source, the destination would not be able to
// change its feedback re whether it could accept the drop, and so the
// source's behavior on drop will not be consistent.)

struct MotionEventData {
    MotionEventData(GtkWidget *aWidget, GdkEvent *aEvent)
        : mWidget(aWidget), mEvent(gdk_event_copy(aEvent))
    {
        MOZ_COUNT_CTOR(MotionEventData);
        g_object_ref(mWidget);
    }
    ~MotionEventData()
    {
        MOZ_COUNT_DTOR(MotionEventData);
        g_object_unref(mWidget);
        gdk_event_free(mEvent);
    }
    GtkWidget *mWidget;
    GdkEvent *mEvent;
};

static void
DestroyMotionEventData(gpointer data)
{
    delete static_cast<MotionEventData*>(data);
}

static gboolean
DispatchMotionEventCopy(gpointer aData)
{
    MotionEventData *data = static_cast<MotionEventData*>(aData);

    // Clear the timer id before OnSourceGrabEventAfter is called during event dispatch.
    sMotionEventTimerID = 0;

    // If there is no longer a grab on the widget, then the drag is over and
    // there is no need to continue drag motion.
    if (gtk_grab_get_current() == data->mWidget) {
        gtk_propagate_event(data->mWidget, data->mEvent);
    }

    // Cancel this timer;
    // We've already started another if the motion event was dispatched.
    return FALSE;
}

static void
OnSourceGrabEventAfter(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    if (event->type != GDK_MOTION_NOTIFY)
        return;

    if (sMotionEventTimerID) {
        g_source_remove(sMotionEventTimerID);
    }

    MotionEventData *data = new MotionEventData(widget, event);

    // G_PRIORITY_DEFAULT_IDLE is lower priority than GDK's redraw idle source
    // and lower than GTK's idle source that sends drag position messages after
    // motion-notify signals.
    //
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#drag-and-drop-processing-model
    // recommends an interval of 350ms +/- 200ms.
    sMotionEventTimerID = 
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 350,
                           DispatchMotionEventCopy, data, DestroyMotionEventData);
}

// nsIDragService

NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                 nsISupportsArray * aArrayTransferables,
                                 nsIScriptableRegion * aRegion,
                                 PRUint32 aActionType)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::InvokeDragSession"));
    nsresult rv = nsBaseDragService::InvokeDragSession(aDOMNode,
                                                       aArrayTransferables,
                                                       aRegion, aActionType);
    NS_ENSURE_SUCCESS(rv, rv);

    // make sure that we have an array of transferables to use
    if (!aArrayTransferables)
        return NS_ERROR_INVALID_ARG;
    // set our reference to the transferables.  this will also addref
    // the transferables since we're going to hang onto this beyond the
    // length of this call
    mSourceDataItems = aArrayTransferables;
    // get the list of items we offer for drags
    GtkTargetList *sourceList = GetSourceList();

    if (!sourceList)
        return NS_OK;

    // stored temporarily until the drag-begin signal has been received
    mSourceRegion = aRegion;

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
    event.button.time = nsWindow::sLastButtonPressTime;

    // start our drag.
    GdkDragContext *context = gtk_drag_begin(mHiddenWidget,
                                             sourceList,
                                             action,
                                             1,
                                             &event);

    mSourceRegion = nsnull;

    if (context) {
        // GTK uses another hidden window for receiving mouse events.
        mGrabWidget = gtk_grab_get_current();
        if (mGrabWidget) {
            g_object_ref(mGrabWidget);
            // Only motion events are required but connect to
            // "event-after" as this is never blocked by other handlers.
            g_signal_connect(mGrabWidget, "event-after",
                             G_CALLBACK(OnSourceGrabEventAfter), NULL);
        }
    }
    else {
        rv = NS_ERROR_FAILURE;
    }

    gtk_target_list_unref(sourceList);

    StartDragSession();

    return rv;
}

PRBool
nsDragService::SetAlphaPixmap(gfxASurface *aSurface,
                                 GdkDragContext *aContext,
                                 PRInt32 aXOffset,
                                 PRInt32 aYOffset,
                                 const nsIntRect& dragRect)
{
    GdkScreen* screen = gtk_widget_get_screen(mHiddenWidget);

    // Transparent drag icons need, like a lot of transparency-related things,
    // a compositing X window manager
    if (!gdk_screen_is_composited(screen))
      return PR_FALSE;

    GdkColormap* alphaColormap = gdk_screen_get_rgba_colormap(screen);
    if (!alphaColormap)
      return PR_FALSE;

    GdkPixmap* pixmap = gdk_pixmap_new(NULL, dragRect.width, dragRect.height,
                                       gdk_colormap_get_visual(alphaColormap)->depth);
    if (!pixmap)
      return PR_FALSE;

    gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), alphaColormap);

    // Make a gfxXlibSurface wrapped around the pixmap to render on
    nsRefPtr<gfxASurface> xPixmapSurface =
         nsWindow::GetSurfaceForGdkDrawable(GDK_DRAWABLE(pixmap),
                                            dragRect.Size());
    if (!xPixmapSurface)
      return PR_FALSE;

    nsRefPtr<gfxContext> xPixmapCtx = new gfxContext(xPixmapSurface);

    // Clear it...
    xPixmapCtx->SetOperator(gfxContext::OPERATOR_CLEAR);
    xPixmapCtx->Paint();

    // ...and paint the drag image with translucency
    xPixmapCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    xPixmapCtx->SetSource(aSurface);
    xPixmapCtx->Paint(DRAG_IMAGE_ALPHA_LEVEL);

    // The drag transaction addrefs the pixmap, so we can just unref it from us here
    gtk_drag_set_icon_pixmap(aContext, alphaColormap, pixmap, NULL,
                             aXOffset, aYOffset);
    g_object_unref(pixmap);
    return PR_TRUE;
}

NS_IMETHODIMP
nsDragService::StartDragSession()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::StartDragSession"));
    return nsBaseDragService::StartDragSession();
}
 
NS_IMETHODIMP
nsDragService::EndDragSession(PRBool aDoneDrag)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::EndDragSession %d",
                                   aDoneDrag));

    if (mGrabWidget) {
        g_signal_handlers_disconnect_by_func(mGrabWidget,
             FuncToGpointer(OnSourceGrabEventAfter), NULL);
        g_object_unref(mGrabWidget);
        mGrabWidget = NULL;

        if (sMotionEventTimerID) {
            g_source_remove(sMotionEventTimerID);
            sMotionEventTimerID = 0;
        }
    }

    // unset our drag action
    SetDragAction(DRAGDROP_ACTION_NONE);
    return nsBaseDragService::EndDragSession(aDoneDrag);
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
            const char *data = reinterpret_cast<char*>(mTargetDragData);
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
        // find a matching flavor
        for (i = 0; i < cnt; ++i) {
            nsCOMPtr<nsISupports> genericWrapper;
            flavorList->GetElementAt(i, getter_AddRefs(genericWrapper));
            nsCOMPtr<nsISupportsCString> currentFlavor;
            currentFlavor = do_QueryInterface(genericWrapper);
            if (!currentFlavor)
                continue;

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
            if (!item)
                continue;

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
                continue;
            }
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("succeeded.\n"));
            rv = aTransferable->SetTransferData(flavorStr,data,tmpDataLen);
            if (NS_FAILED(rv)) {
                PR_LOG(sDragLm,
                       PR_LOG_DEBUG,
                       ("fail to set transfer data into transferable!\n"));
                continue;
            }
            // ok, we got the data
            return NS_OK;
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
                   static_cast<const char*>(flavorStr), gdkFlavor));
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

                // Dragging and dropping from the file manager would cause us 
                // to parse the source text as a nsILocalFile URL.
                if ( strcmp(flavorStr, kFileMime) == 0 ) {
                    gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        const char* text = static_cast<char*>(mTargetDragData);
                        PRUnichar* convertedText = nsnull;
                        PRInt32 convertedTextLen = 0;

                        GetTextUriListItem(text, mTargetDragDataLen, aItemIndex,
                                           &convertedText, &convertedTextLen);

                        if (convertedText) {
                            nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
                            nsCOMPtr<nsIURI> fileURI;
                            nsresult rv = ioService->NewURI(NS_ConvertUTF16toUTF8(convertedText),
                                                            nsnull, nsnull, getter_AddRefs(fileURI));
                            if (NS_SUCCEEDED(rv)) {
                                nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
                                if (NS_SUCCEEDED(rv)) {
                                    nsCOMPtr<nsIFile> file;
                                    rv = fileURL->GetFile(getter_AddRefs(file));
                                    if (NS_SUCCEEDED(rv)) {
                                        // The common wrapping code at the end of 
                                        // this function assumes the data is text
                                        // and calls text-specific operations.
                                        // Make a secret hideout here for nsILocalFile
                                        // objects and return early.
                                        aTransferable->SetTransferData(flavorStr, file,
                                                                       convertedTextLen);
                                        g_free(convertedText);
                                        return NS_OK;
                                    }
                                }
                            }
                            g_free(convertedText);
                        }
                        continue;
                    }
                }

                // if we are looking for text/unicode and we fail to find it
                // on the clipboard first, try again with text/plain. If that
                // is present, convert it to unicode.
                if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
                    PR_LOG(sDragLm, PR_LOG_DEBUG,
                           ("we were looking for text/unicode... \
                           trying with text/plain;charset=utf-8\n"));
                    gdkFlavor = gdk_atom_intern(gTextPlainUTF8Type, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        PR_LOG(sDragLm, PR_LOG_DEBUG, ("Got textplain data\n"));
                        const char* castedText =
                                    reinterpret_cast<char*>(mTargetDragData);
                        PRUnichar* convertedText = nsnull;
                        NS_ConvertUTF8toUTF16 ucs2string(castedText,
                                                         mTargetDragDataLen);
                        convertedText = ToNewUnicode(ucs2string);
                        if ( convertedText ) {
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("successfully converted plain text \
                                   to unicode.\n"));
                            // out with the old, in with the new
                            g_free(mTargetDragData);
                            mTargetDragData = convertedText;
                            mTargetDragDataLen = ucs2string.Length() * 2;
                            dataFound = PR_TRUE;
                        } // if plain text data on clipboard
                    } else {
                        PR_LOG(sDragLm, PR_LOG_DEBUG,
                               ("we were looking for text/unicode... \
                               trying again with text/plain\n"));
                        gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
                        GetTargetDragData(gdkFlavor);
                        if (mTargetDragData) {
                            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Got textplain data\n"));
                            const char* castedText =
                                        reinterpret_cast<char*>(mTargetDragData);
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
                    } // if plain text charset=utf-8 flavor present
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
                                   reinterpret_cast<char*>(mTargetDragData);
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
                                  reinterpret_cast<char*>(mTargetDragData);
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
                             reinterpret_cast<int*>(&mTargetDragDataLen));
        
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
        /* Bug 331198 */
        GdkAtom atom = GDK_POINTER_TO_ATOM(tmp->data);
        gchar *name = NULL;
        name = gdk_atom_name(atom);
        PR_LOG(sDragLm, PR_LOG_DEBUG,
               ("checking %s against %s\n", name, aDataFlavor));
        if (name && (strcmp(name, aDataFlavor) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("good!\n"));
            *_retval = PR_TRUE;
        }
        // check for automatic text/uri-list -> text/x-moz-url mapping
        if (!*_retval && 
            name &&
            (strcmp(name, gTextUriListType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's text/uri-list and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = PR_TRUE;
        }
        // check for automatic _NETSCAPE_URL -> text/x-moz-url mapping
        if (!*_retval && 
            name &&
            (strcmp(name, gMozUrlType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0)) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's _NETSCAPE_URL and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = PR_TRUE;
        }
        // check for auto text/plain -> text/unicode mapping
        if (!*_retval && 
            name &&
            (strcmp(name, kTextMime) == 0) &&
            ((strcmp(aDataFlavor, kUnicodeMime) == 0) ||
             (strcmp(aDataFlavor, kFileMime) == 0))) {
            PR_LOG(sDragLm, PR_LOG_DEBUG,
                   ("good! ( it's text plain and we're checking \
                   against text/unicode or application/x-moz-file)\n"));
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
        /* Bug 331198 */
        GdkAtom atom = GDK_POINTER_TO_ATOM(tmp->data);
        gchar *name = NULL;
        name = gdk_atom_name(atom);
        if (name && strcmp(name, gMimeListType) == 0)
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
    g_free(mTargetDragData);
    mTargetDragData = 0;
    mTargetDragDataLen = 0;
}

GtkTargetList *
nsDragService::GetSourceList(void)
{
    if (!mSourceDataItems)
        return NULL;
    nsTArray<GtkTargetEntry*> targetArray;
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
        /* Bug 331198 */
        listTarget->info = NS_PTR_TO_UINT32(listAtom);
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
                            /* Bug 331198 */
                            listTarget->info = NS_PTR_TO_UINT32(listAtom);
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
                        /* Bug 331198 */
                        target->info = NS_PTR_TO_UINT32(atom);
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
                            GdkAtom plainUTF8Atom =
                              gdk_atom_intern(gTextPlainUTF8Type, FALSE);
                            GtkTargetEntry *plainUTF8Target =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            plainUTF8Target->target = g_strdup(gTextPlainUTF8Type);
                            plainUTF8Target->flags = 0;
                            /* Bug 331198 */
                            plainUTF8Target->info = NS_PTR_TO_UINT32(plainUTF8Atom);
                            PR_LOG(sDragLm, PR_LOG_DEBUG,
                                   ("automatically adding target %s with \
                                   id %ld\n", plainUTF8Target->target, plainUTF8Atom));
                            targetArray.AppendElement(plainUTF8Target);

                            // get the atom for the ASCII string
                            GdkAtom plainAtom =
                              gdk_atom_intern(kTextMime, FALSE);
                            GtkTargetEntry *plainTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            plainTarget->target = g_strdup(kTextMime);
                            plainTarget->flags = 0;
                            /* Bug 331198 */
                            plainTarget->info = NS_PTR_TO_UINT32(plainAtom);
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
                            /* Bug 331198 */
                            urlTarget->info = NS_PTR_TO_UINT32(urlAtom);
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
    targetCount = targetArray.Length();
    if (targetCount) {
        // allocate space to create the list of valid targets
        targets =
          (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry) * targetCount);
        PRUint32 targetIndex;
        for ( targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
            GtkTargetEntry *disEntry = targetArray.ElementAt(targetIndex);
            // this is a string reference but it will be freed later.
            targets[targetIndex].target = disEntry->target;
            targets[targetIndex].flags = disEntry->flags;
            targets[targetIndex].info = disEntry->info;
        }
        targetList = gtk_target_list_new(targets, targetCount);
        // clean up the target list
        for (PRUint32 cleanIndex = 0; cleanIndex < targetCount; ++cleanIndex) {
            GtkTargetEntry *thisTarget = targetArray.ElementAt(cleanIndex);
            g_free(thisTarget->target);
            g_free(thisTarget);
        }
        g_free(targets);
    }
    return targetList;
}

void
nsDragService::SourceEndDragSession(GdkDragContext *aContext,
                                    gint            aResult)
{
    // this just releases the list of data items that we provide
    mSourceDataItems = nsnull;

    if (!mDoingDrag)
        return; // EndDragSession() was already called on drop or drag-failed

    gint x, y;
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
      gdk_display_get_pointer(display, NULL, &x, &y, NULL);
      SetDragEndPoint(nsIntPoint(x, y));
    }

    // Either the drag was aborted or the drop occurred outside the app.
    // The dropEffect of mDataTransfer is not updated for motion outside the
    // app, but is needed for the dragend event, so set it now.

    PRUint32 dropEffect;

    if (aResult == MOZ_GTK_DRAG_RESULT_SUCCESS) {

        // With GTK+ versions 2.10.x and prior the drag may have been
        // cancelled (but no drag-failed signal would have been sent).
        // aContext->dest_window will be non-NULL only if the drop was sent.
        GdkDragAction action =
            aContext->dest_window ? aContext->action : (GdkDragAction)0;

        // Only one bit of action should be set, but, just in case someone
        // does something funny, erring away from MOVE, and not recording
        // unusual action combinations as NONE.
        if (!action)
            dropEffect = DRAGDROP_ACTION_NONE;
        else if (action & GDK_ACTION_COPY)
            dropEffect = DRAGDROP_ACTION_COPY;
        else if (action & GDK_ACTION_LINK)
            dropEffect = DRAGDROP_ACTION_LINK;
        else if (action & GDK_ACTION_MOVE)
            dropEffect = DRAGDROP_ACTION_MOVE;
        else
            dropEffect = DRAGDROP_ACTION_COPY;

    } else {

        dropEffect = DRAGDROP_ACTION_NONE;

        if (aResult != MOZ_GTK_DRAG_RESULT_NO_TARGET) {
            mUserCancelled = PR_TRUE;
        }
    }

    nsCOMPtr<nsIDOMNSDataTransfer> dataTransfer =
        do_QueryInterface(mDataTransfer);

    if (dataTransfer) {
        dataTransfer->SetDropEffectInt(dropEffect);
    }

    // Inform the drag session that we're ending the drag.
    EndDragSession(PR_TRUE);
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
                PRUnichar* castedUnicode = reinterpret_cast<PRUnichar*>
                                                           (tmpData);
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

    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item;
    item = do_QueryInterface(genericItem);
    if (item) {
        // if someone was asking for text/plain, lookup unicode instead so
        // we can convert it.
        PRBool needToDoConversionToPlainText = PR_FALSE;
        const char* actualFlavor = mimeFlavor;
        if (strcmp(mimeFlavor, kTextMime) == 0 ||
            strcmp(mimeFlavor, gTextPlainUTF8Type) == 0) {
            actualFlavor = kUnicodeMime;
            needToDoConversionToPlainText = PR_TRUE;
        }
        // if someone was asking for _NETSCAPE_URL we need to convert to
        // plain text but we also need to look for x-moz-url
        else if (strcmp(mimeFlavor, gMozUrlType) == 0) {
            actualFlavor = kURLMime;
            needToDoConversionToPlainText = PR_TRUE;
        }
        // if someone was asking for text/uri-list we need to convert to
        // plain text.
        else if (strcmp(mimeFlavor, gTextUriListType) == 0) {
            actualFlavor = gTextUriListType;
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
                PRUnichar* castedUnicode = reinterpret_cast<PRUnichar*>
                                                           (tmpData);
                PRInt32 plainTextLen = 0;
                if (strcmp(mimeFlavor, gTextPlainUTF8Type) == 0) {
                    plainTextData =
                        ToNewUTF8String(
                            nsDependentString(castedUnicode, tmpDataLen / 2),
                            (PRUint32*)&plainTextLen);
                } else {
                    nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText(
                                        castedUnicode,
                                        tmpDataLen / 2,
                                        &plainTextData,
                                        &plainTextLen);
                }
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
        } else {
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
        }
    }
}

void nsDragService::SetDragIcon(GdkDragContext* aContext)
{
    if (!mHasImage && !mSelection)
        return;

    nsIntRect dragRect;
    nsPresContext* pc;
    nsRefPtr<gfxASurface> surface;
    DrawDrag(mSourceNode, mSourceRegion, mScreenX, mScreenY,
             &dragRect, getter_AddRefs(surface), &pc);
    if (!pc)
        return;

    PRInt32 sx = mScreenX, sy = mScreenY;
    ConvertToUnscaledDevPixels(pc, &sx, &sy);

    PRInt32 offsetX = sx - dragRect.x;
    PRInt32 offsetY = sy - dragRect.y;

    // If a popup is set as the drag image, use its widget. Otherwise, use
    // the surface that DrawDrag created.
    if (mDragPopup) {
        GtkWidget* gtkWidget = nsnull;
        nsIFrame* frame = mDragPopup->GetPrimaryFrame();
        if (frame) {
            // DrawDrag ensured that this is a popup frame.
            nsCOMPtr<nsIWidget> widget = frame->GetNearestWidget();
            if (widget) {
                gtkWidget = (GtkWidget *)widget->GetNativeData(NS_NATIVE_SHELLWIDGET);
                if (gtkWidget) {
                    OpenDragPopup();
                    gtk_drag_set_icon_widget(aContext, gtkWidget, offsetX, offsetY);
                }
            }
        }
    }
    else if (surface) {
        if (!SetAlphaPixmap(surface, aContext, offsetX, offsetY, dragRect)) {
            GdkPixbuf* dragPixbuf =
              nsImageToPixbuf::SurfaceToPixbuf(surface, dragRect.width, dragRect.height);
            if (dragPixbuf) {
                gtk_drag_set_icon_pixbuf(aContext, dragPixbuf, offsetX, offsetY);
                g_object_unref(dragPixbuf);
            }
        }
    }
}

static void
invisibleSourceDragBegin(GtkWidget        *aWidget,
                         GdkDragContext   *aContext,
                         gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleSourceDragBegin"));
    nsDragService *dragService = (nsDragService *)aData;

    dragService->SetDragIcon(aContext);
}

static void
invisibleSourceDragDataGet(GtkWidget        *aWidget,
                           GdkDragContext   *aContext,
                           GtkSelectionData *aSelectionData,
                           guint             aInfo,
                           guint32           aTime,
                           gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleSourceDragDataGet"));
    nsDragService *dragService = (nsDragService *)aData;
    dragService->SourceDataGet(aWidget, aContext,
                               aSelectionData, aInfo, aTime);
}

static gboolean
invisibleSourceDragFailed(GtkWidget        *aWidget,
                          GdkDragContext   *aContext,
                          gint              aResult,
                          gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleSourceDragFailed %i", aResult));
    nsDragService *dragService = (nsDragService *)aData;
    // End the drag session now (rather than waiting for the drag-end signal)
    // so that operations performed on dropEffect == none can start immediately
    // rather than waiting for the drag-failed animation to finish.
    dragService->SourceEndDragSession(aContext, aResult);

    // We should return TRUE to disable the drag-failed animation iff the
    // source performed an operation when dropEffect was none, but the handler
    // of the dragend DOM event doesn't provide this information.
    return FALSE;
}

static void
invisibleSourceDragEnd(GtkWidget        *aWidget,
                       GdkDragContext   *aContext,
                       gpointer          aData)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invisibleSourceDragEnd"));
    nsDragService *dragService = (nsDragService *)aData;

    // The drag has ended.  Release the hostages!
    dragService->SourceEndDragSession(aContext, MOZ_GTK_DRAG_RESULT_SUCCESS);
}

