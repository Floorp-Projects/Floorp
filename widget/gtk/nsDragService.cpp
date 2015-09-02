/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "nsPrimitiveHelpers.h"
#include "prtime.h"
#include "prthread.h"
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "nsCRT.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Services.h"

#include "gfxASurface.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "nsImageToPixbuf.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "nsGtkUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfxPlatform.h"
#include "nsScreenGtk.h"

using namespace mozilla;
using namespace mozilla::gfx;

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

static PRLogModuleInfo *sDragLm = nullptr;

// data used for synthetic periodic motion events sent to the source widget
// grabbing real events for the drag.
static guint sMotionEventTimerID;
static GdkEvent *sMotionEvent;
static GtkWidget *sGrabWidget;

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
    : mScheduledTask(eDragTaskNone)
    , mTaskSource(0)
{
    // We have to destroy the hidden widget before the event loop stops
    // running.
    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    obsServ->AddObserver(this, "quit-application", false);

    // our hidden source widget
#if (MOZ_WIDGET_GTK == 2)
    mHiddenWidget = gtk_window_new(GTK_WINDOW_POPUP);
#else
    // Using an offscreen window works around bug 983843.
    mHiddenWidget = gtk_offscreen_window_new();
#endif
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
                                                      this, nullptr),
                                       FALSE);
    }

    // set up our logging module
    if (!sDragLm)
        sDragLm = PR_NewLogModule("nsDragService");
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::nsDragService"));
    mCanDrop = false;
    mTargetDragDataReceived = false;
    mTargetDragData = 0;
    mTargetDragDataLen = 0;
}

nsDragService::~nsDragService()
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::~nsDragService"));
    if (mTaskSource)
        g_source_remove(mTaskSource);

}

NS_IMPL_ISUPPORTS_INHERITED(nsDragService, nsBaseDragService, nsIObserver)

/* static */ nsDragService*
nsDragService::GetInstance()
{
    static const nsIID iid = NS_DRAGSERVICE_CID;
    nsCOMPtr<nsIDragService> dragService = do_GetService(iid);
    return static_cast<nsDragService*>(dragService.get());
    // We rely on XPCOM keeping a reference to the service.
}

// nsIObserver

NS_IMETHODIMP
nsDragService::Observe(nsISupports *aSubject, const char *aTopic,
                       const char16_t *aData)
{
  if (!nsCRT::strcmp(aTopic, "quit-application")) {
    MOZ_LOG(sDragLm, LogLevel::Debug,
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

static gboolean
DispatchMotionEventCopy(gpointer aData)
{
    // Clear the timer id before OnSourceGrabEventAfter is called during event
    // dispatch.
    sMotionEventTimerID = 0;

    GdkEvent *event = sMotionEvent;
    sMotionEvent = nullptr;
    // If there is no longer a grab on the widget, then the drag is over and
    // there is no need to continue drag motion.
    if (gtk_widget_has_grab(sGrabWidget)) {
        gtk_propagate_event(sGrabWidget, event);
    }
    gdk_event_free(event);

    // Cancel this timer;
    // We've already started another if the motion event was dispatched.
    return FALSE;
}

static void
OnSourceGrabEventAfter(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    // If there is no longer a grab on the widget, then the drag motion is
    // over (though the data may not be fetched yet).
    if (!gtk_widget_has_grab(sGrabWidget))
        return;

    if (event->type == GDK_MOTION_NOTIFY) {
        if (sMotionEvent) {
            gdk_event_free(sMotionEvent);
        }
        sMotionEvent = gdk_event_copy(event);

        // Update the cursor position.  The last of these recorded gets used for
        // the NS_DRAGDROP_END event.
        nsDragService *dragService = static_cast<nsDragService*>(user_data);
        dragService->SetDragEndPoint(nsIntPoint(event->motion.x_root,
                                                event->motion.y_root));
    } else if (sMotionEvent && (event->type == GDK_KEY_PRESS ||
                                event->type == GDK_KEY_RELEASE)) {
        // Update modifier state from key events.
        sMotionEvent->motion.state = event->key.state;
    } else {
        return;
    }

    if (sMotionEventTimerID) {
        g_source_remove(sMotionEventTimerID);
    }

    // G_PRIORITY_DEFAULT_IDLE is lower priority than GDK's redraw idle source
    // and lower than GTK's idle source that sends drag position messages after
    // motion-notify signals.
    //
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#drag-and-drop-processing-model
    // recommends an interval of 350ms +/- 200ms.
    sMotionEventTimerID = 
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 350,
                           DispatchMotionEventCopy, nullptr, nullptr);
}

static GtkWindow*
GetGtkWindow(nsIDOMDocument *aDocument)
{
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
    if (!doc)
        return nullptr;

    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    if (!presShell)
        return nullptr;

    nsRefPtr<nsViewManager> vm = presShell->GetViewManager();
    if (!vm)
        return nullptr;

    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    if (!widget)
        return nullptr;

    GtkWidget *gtkWidget =
        static_cast<nsWindow*>(widget.get())->GetMozContainerWidget();
    if (!gtkWidget)
        return nullptr;

    GtkWidget *toplevel = nullptr;
    toplevel = gtk_widget_get_toplevel(gtkWidget);
    if (!GTK_IS_WINDOW(toplevel))
        return nullptr;

    return GTK_WINDOW(toplevel);
}   

// nsIDragService

NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                 nsISupportsArray * aArrayTransferables,
                                 nsIScriptableRegion * aRegion,
                                 uint32_t aActionType)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::InvokeDragSession"));

    // If the previous source drag has not yet completed, signal handlers need
    // to be removed from sGrabWidget and dragend needs to be dispatched to
    // the source node, but we can't call EndDragSession yet because we don't
    // know whether or not the drag succeeded.
    if (mSourceNode)
        return NS_ERROR_NOT_AVAILABLE;

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

    // Create a fake event for the drag so we can pass the time (so to speak).
    // If we don't do this, then, when the timestamp for the pending button
    // release event is used for the ungrab, the ungrab can fail due to the
    // timestamp being _earlier_ than CurrentTime.
    GdkEvent event;
    memset(&event, 0, sizeof(GdkEvent));
    event.type = GDK_BUTTON_PRESS;
    event.button.window = gtk_widget_get_window(mHiddenWidget);
    event.button.time = nsWindow::GetLastUserInputTime();

    // Put the drag widget in the window group of the source node so that the
    // gtk_grab_add during gtk_drag_begin is effective.
    // gtk_window_get_group(nullptr) returns the default window group.
    GtkWindowGroup *window_group =
        gtk_window_get_group(GetGtkWindow(mSourceDocument));
    gtk_window_group_add_window(window_group,
                                GTK_WINDOW(mHiddenWidget));

#if (MOZ_WIDGET_GTK == 3)
    // Get device for event source
    GdkDisplay *display = gdk_display_get_default();
    GdkDeviceManager *device_manager = gdk_display_get_device_manager(display);
    event.button.device = gdk_device_manager_get_client_pointer(device_manager);
#endif
  
    // start our drag.
    GdkDragContext *context = gtk_drag_begin(mHiddenWidget,
                                             sourceList,
                                             action,
                                             1,
                                             &event);

    mSourceRegion = nullptr;

    if (context) {
        StartDragSession();

        // GTK uses another hidden window for receiving mouse events.
        sGrabWidget = gtk_window_group_get_current_grab(window_group);
        if (sGrabWidget) {
            g_object_ref(sGrabWidget);
            // Only motion and key events are required but connect to
            // "event-after" as this is never blocked by other handlers.
            g_signal_connect(sGrabWidget, "event-after",
                             G_CALLBACK(OnSourceGrabEventAfter), this);
        }
        // We don't have a drag end point yet.
        mEndDragPoint = nsIntPoint(-1, -1);
    }
    else {
        rv = NS_ERROR_FAILURE;
    }

    gtk_target_list_unref(sourceList);

    return rv;
}

bool
nsDragService::SetAlphaPixmap(SourceSurface *aSurface,
                              GdkDragContext *aContext,
                              int32_t aXOffset,
                              int32_t aYOffset,
                              const nsIntRect& dragRect)
{
    GdkScreen* screen = gtk_widget_get_screen(mHiddenWidget);

    // Transparent drag icons need, like a lot of transparency-related things,
    // a compositing X window manager
    if (!gdk_screen_is_composited(screen))
      return false;

#if (MOZ_WIDGET_GTK == 2)
    GdkColormap* alphaColormap = gdk_screen_get_rgba_colormap(screen);
    if (!alphaColormap)
      return false;

    GdkPixmap* pixmap = gdk_pixmap_new(nullptr, dragRect.width, dragRect.height,
                                       gdk_colormap_get_visual(alphaColormap)->depth);
    if (!pixmap)
      return false;

    gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), alphaColormap);

    // Make a gfxXlibSurface wrapped around the pixmap to render on
    nsRefPtr<gfxASurface> xPixmapSurface =
         nsWindow::GetSurfaceForGdkDrawable(GDK_DRAWABLE(pixmap),
                                            dragRect.Size());
    if (!xPixmapSurface)
      return false;

    RefPtr<DrawTarget> dt =
    gfxPlatform::GetPlatform()->
      CreateDrawTargetForSurface(xPixmapSurface, IntSize(dragRect.width, dragRect.height));
    if (!dt)
      return false;

    // Clear it...
    dt->ClearRect(Rect(0, 0, dragRect.width, dragRect.height));

    // ...and paint the drag image with translucency
    dt->DrawSurface(aSurface,
                    Rect(0, 0, dragRect.width, dragRect.height),
                    Rect(0, 0, dragRect.width, dragRect.height),
                    DrawSurfaceOptions(),
                    DrawOptions(DRAG_IMAGE_ALPHA_LEVEL, CompositionOp::OP_SOURCE));

    // The drag transaction addrefs the pixmap, so we can just unref it from us here
    gtk_drag_set_icon_pixmap(aContext, alphaColormap, pixmap, nullptr,
                             aXOffset, aYOffset);
    g_object_unref(pixmap);
    return true;
#else
#ifdef cairo_image_surface_create
#error "Looks like we're including Mozilla's cairo instead of system cairo"
#endif
    // TODO: grab X11 pixmap or image data instead of expensive readback.
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                       dragRect.width,
                                                       dragRect.height);
    if (!surf)
        return false;

    RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->
        CreateDrawTargetForData(cairo_image_surface_get_data(surf),
                                dragRect.Size(),
                                cairo_image_surface_get_stride(surf),
                                SurfaceFormat::B8G8R8A8);
    if (!dt)
        return false;

    dt->ClearRect(Rect(0, 0, dragRect.width, dragRect.height));
    dt->DrawSurface(aSurface,
                    Rect(0, 0, dragRect.width, dragRect.height),
                    Rect(0, 0, dragRect.width, dragRect.height),
                    DrawSurfaceOptions(),
                    DrawOptions(DRAG_IMAGE_ALPHA_LEVEL, CompositionOp::OP_SOURCE));

    cairo_surface_mark_dirty(surf);
    cairo_surface_set_device_offset(surf, -aXOffset, -aYOffset);

    // Ensure that the surface is drawn at the correct scale on HiDPI displays.
    static auto sCairoSurfaceSetDeviceScalePtr =
        (void (*)(cairo_surface_t*,double,double))
        dlsym(RTLD_DEFAULT, "cairo_surface_set_device_scale");
    if (sCairoSurfaceSetDeviceScalePtr) {
        gint scale = nsScreenGtk::GetGtkMonitorScaleFactor();
        sCairoSurfaceSetDeviceScalePtr(surf, scale, scale);
    }

    gtk_drag_set_icon_surface(aContext, surf);
    cairo_surface_destroy(surf);
    return true;
#endif
}

NS_IMETHODIMP
nsDragService::StartDragSession()
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::StartDragSession"));
    return nsBaseDragService::StartDragSession();
}
 
NS_IMETHODIMP
nsDragService::EndDragSession(bool aDoneDrag)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::EndDragSession %d",
                                   aDoneDrag));

    if (sGrabWidget) {
        g_signal_handlers_disconnect_by_func(sGrabWidget,
             FuncToGpointer(OnSourceGrabEventAfter), this);
        g_object_unref(sGrabWidget);
        sGrabWidget = nullptr;

        if (sMotionEventTimerID) {
            g_source_remove(sMotionEventTimerID);
            sMotionEventTimerID = 0;
        }
        if (sMotionEvent) {
            gdk_event_free(sMotionEvent);
            sMotionEvent = nullptr;
        }
    }

    // unset our drag action
    SetDragAction(DRAGDROP_ACTION_NONE);
    
    // We're done with the drag context.
    mTargetDragContextForRemote = nullptr;

    return nsBaseDragService::EndDragSession(aDoneDrag);
}

// nsIDragSession
NS_IMETHODIMP
nsDragService::SetCanDrop(bool aCanDrop)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::SetCanDrop %d",
                                   aCanDrop));
    mCanDrop = aCanDrop;
    return NS_OK;
}

NS_IMETHODIMP
nsDragService::GetCanDrop(bool *aCanDrop)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::GetCanDrop"));
    *aCanDrop = mCanDrop;
    return NS_OK;
}

// count the number of URIs in some text/uri-list format data.
static uint32_t
CountTextUriListItems(const char *data,
                      uint32_t datalen)
{
    const char *p = data;
    const char *endPtr = p + datalen;
    uint32_t count = 0;

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
                   uint32_t datalen,
                   uint32_t aItemIndex,
                   char16_t **convertedText,
                   int32_t *convertedTextLen)
{
    const char *p = data;
    const char *endPtr = p + datalen;
    unsigned int count = 0;

    *convertedText = nullptr;
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
nsDragService::GetNumDropItems(uint32_t * aNumItems)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::GetNumDropItems"));

    if (!mTargetWidget) {
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("*** warning: GetNumDropItems \
               called without a valid target widget!\n"));
        *aNumItems = 0;
        return NS_OK;
    }

    bool isList = IsTargetContextList();
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
    MOZ_LOG(sDragLm, LogLevel::Debug, ("%d items", *aNumItems));
    return NS_OK;
}


NS_IMETHODIMP
nsDragService::GetData(nsITransferable * aTransferable,
                       uint32_t aItemIndex)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::GetData %d", aItemIndex));

    // make sure that we have a transferable
    if (!aTransferable)
        return NS_ERROR_INVALID_ARG;

    if (!mTargetWidget) {
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("*** warning: GetData \
               called without a valid target widget!\n"));
        return NS_ERROR_FAILURE;
    }

    // get flavor list that includes all acceptable flavors (including
    // ones obtained through conversion). Flavors are nsISupportsStrings
    // so that they can be seen from JS.
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult rv = aTransferable->FlavorsTransferableCanImport(
                        getter_AddRefs(flavorList));
    if (NS_FAILED(rv))
        return rv;

    // count the number of flavors
    uint32_t cnt;
    flavorList->Count(&cnt);
    unsigned int i;

    // check to see if this is an internal list
    bool isList = IsTargetContextList();

    if (isList) {
        MOZ_LOG(sDragLm, LogLevel::Debug, ("it's a list..."));
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
            MOZ_LOG(sDragLm,
                   LogLevel::Debug,
                   ("flavor is %s\n", (const char *)flavorStr));
            // get the item with the right index
            nsCOMPtr<nsISupports> genericItem;
            mSourceDataItems->GetElementAt(aItemIndex,
                                           getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> item(do_QueryInterface(genericItem));
            if (!item)
                continue;

            nsCOMPtr<nsISupports> data;
            uint32_t tmpDataLen = 0;
            MOZ_LOG(sDragLm, LogLevel::Debug,
                   ("trying to get transfer data for %s\n",
                   (const char *)flavorStr));
            rv = item->GetTransferData(flavorStr,
                                       getter_AddRefs(data),
                                       &tmpDataLen);
            if (NS_FAILED(rv)) {
                MOZ_LOG(sDragLm, LogLevel::Debug, ("failed.\n"));
                continue;
            }
            MOZ_LOG(sDragLm, LogLevel::Debug, ("succeeded.\n"));
            rv = aTransferable->SetTransferData(flavorStr,data,tmpDataLen);
            if (NS_FAILED(rv)) {
                MOZ_LOG(sDragLm,
                       LogLevel::Debug,
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
            MOZ_LOG(sDragLm, LogLevel::Debug,
                   ("looking for data in type %s, gdk flavor %ld\n",
                   static_cast<const char*>(flavorStr), gdkFlavor));
            bool dataFound = false;
            if (gdkFlavor) {
                GetTargetDragData(gdkFlavor);
            }
            if (mTargetDragData) {
                MOZ_LOG(sDragLm, LogLevel::Debug, ("dataFound = true\n"));
                dataFound = true;
            }
            else {
                MOZ_LOG(sDragLm, LogLevel::Debug, ("dataFound = false\n"));

                // Dragging and dropping from the file manager would cause us 
                // to parse the source text as a nsIFile URL.
                if ( strcmp(flavorStr, kFileMime) == 0 ) {
                    gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (!mTargetDragData) {
                        gdkFlavor = gdk_atom_intern(gTextUriListType, FALSE);
                        GetTargetDragData(gdkFlavor);
                    }
                    if (mTargetDragData) {
                        const char* text = static_cast<char*>(mTargetDragData);
                        char16_t* convertedText = nullptr;
                        int32_t convertedTextLen = 0;

                        GetTextUriListItem(text, mTargetDragDataLen, aItemIndex,
                                           &convertedText, &convertedTextLen);

                        if (convertedText) {
                            nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
                            nsCOMPtr<nsIURI> fileURI;
                            rv = ioService->NewURI(NS_ConvertUTF16toUTF8(convertedText),
                                                   nullptr, nullptr, getter_AddRefs(fileURI));
                            if (NS_SUCCEEDED(rv)) {
                                nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
                                if (NS_SUCCEEDED(rv)) {
                                    nsCOMPtr<nsIFile> file;
                                    rv = fileURL->GetFile(getter_AddRefs(file));
                                    if (NS_SUCCEEDED(rv)) {
                                        // The common wrapping code at the end of 
                                        // this function assumes the data is text
                                        // and calls text-specific operations.
                                        // Make a secret hideout here for nsIFile
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
                    MOZ_LOG(sDragLm, LogLevel::Debug,
                           ("we were looking for text/unicode... \
                           trying with text/plain;charset=utf-8\n"));
                    gdkFlavor = gdk_atom_intern(gTextPlainUTF8Type, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        MOZ_LOG(sDragLm, LogLevel::Debug, ("Got textplain data\n"));
                        const char* castedText =
                                    reinterpret_cast<char*>(mTargetDragData);
                        char16_t* convertedText = nullptr;
                        NS_ConvertUTF8toUTF16 ucs2string(castedText,
                                                         mTargetDragDataLen);
                        convertedText = ToNewUnicode(ucs2string);
                        if ( convertedText ) {
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("successfully converted plain text \
                                   to unicode.\n"));
                            // out with the old, in with the new
                            g_free(mTargetDragData);
                            mTargetDragData = convertedText;
                            mTargetDragDataLen = ucs2string.Length() * 2;
                            dataFound = true;
                        } // if plain text data on clipboard
                    } else {
                        MOZ_LOG(sDragLm, LogLevel::Debug,
                               ("we were looking for text/unicode... \
                               trying again with text/plain\n"));
                        gdkFlavor = gdk_atom_intern(kTextMime, FALSE);
                        GetTargetDragData(gdkFlavor);
                        if (mTargetDragData) {
                            MOZ_LOG(sDragLm, LogLevel::Debug, ("Got textplain data\n"));
                            const char* castedText =
                                        reinterpret_cast<char*>(mTargetDragData);
                            char16_t* convertedText = nullptr;
                            int32_t convertedTextLen = 0;
                            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(
                                                castedText, mTargetDragDataLen,
                                                &convertedText, &convertedTextLen);
                            if ( convertedText ) {
                                MOZ_LOG(sDragLm, LogLevel::Debug,
                                       ("successfully converted plain text \
                                       to unicode.\n"));
                                // out with the old, in with the new
                                g_free(mTargetDragData);
                                mTargetDragData = convertedText;
                                mTargetDragDataLen = convertedTextLen * 2;
                                dataFound = true;
                            } // if plain text data on clipboard
                        } // if plain text flavor present
                    } // if plain text charset=utf-8 flavor present
                } // if looking for text/unicode

                // if we are looking for text/x-moz-url and we failed to find
                // it on the clipboard, try again with text/uri-list, and then
                // _NETSCAPE_URL
                if (strcmp(flavorStr, kURLMime) == 0) {
                    MOZ_LOG(sDragLm, LogLevel::Debug,
                           ("we were looking for text/x-moz-url...\
                           trying again with text/uri-list\n"));
                    gdkFlavor = gdk_atom_intern(gTextUriListType, FALSE);
                    GetTargetDragData(gdkFlavor);
                    if (mTargetDragData) {
                        MOZ_LOG(sDragLm, LogLevel::Debug,
                               ("Got text/uri-list data\n"));
                        const char *data =
                                   reinterpret_cast<char*>(mTargetDragData);
                        char16_t* convertedText = nullptr;
                        int32_t convertedTextLen = 0;

                        GetTextUriListItem(data, mTargetDragDataLen, aItemIndex,
                                           &convertedText, &convertedTextLen);

                        if ( convertedText ) {
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("successfully converted \
                                   _NETSCAPE_URL to unicode.\n"));
                            // out with the old, in with the new
                            g_free(mTargetDragData);
                            mTargetDragData = convertedText;
                            mTargetDragDataLen = convertedTextLen * 2;
                            dataFound = true;
                        }
                    }
                    else {
                        MOZ_LOG(sDragLm, LogLevel::Debug,
                               ("failed to get text/uri-list data\n"));
                    }
                    if (!dataFound) {
                        MOZ_LOG(sDragLm, LogLevel::Debug,
                               ("we were looking for text/x-moz-url...\
                               trying again with _NETSCAP_URL\n"));
                        gdkFlavor = gdk_atom_intern(gMozUrlType, FALSE);
                        GetTargetDragData(gdkFlavor);
                        if (mTargetDragData) {
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("Got _NETSCAPE_URL data\n"));
                            const char* castedText =
                                  reinterpret_cast<char*>(mTargetDragData);
                            char16_t* convertedText = nullptr;
                            int32_t convertedTextLen = 0;
                            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(castedText, mTargetDragDataLen, &convertedText, &convertedTextLen);
                            if ( convertedText ) {
                                MOZ_LOG(sDragLm,
                                       LogLevel::Debug,
                                       ("successfully converted _NETSCAPE_URL \
                                       to unicode.\n"));
                                // out with the old, in with the new
                                g_free(mTargetDragData);
                                mTargetDragData = convertedText;
                                mTargetDragDataLen = convertedTextLen * 2;
                                dataFound = true;
                            }
                        }
                        else {
                            MOZ_LOG(sDragLm, LogLevel::Debug,
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
                MOZ_LOG(sDragLm, LogLevel::Debug, ("dataFound and converted!\n"));
                break;
            }
        } // if (currentFlavor)
    } // foreach flavor

    return NS_OK;
  
}

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor,
                                     bool *_retval)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::IsDataFlavorSupported %s",
                                   aDataFlavor));
    if (!_retval)
        return NS_ERROR_INVALID_ARG;

    // set this to no by default
    *_retval = false;

    // check to make sure that we have a drag object set, here
    if (!mTargetWidget) {
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("*** warning: IsDataFlavorSupported \
               called without a valid target widget!\n"));
        return NS_OK;
    }

    // check to see if the target context is a list.
    bool isList = IsTargetContextList();
    // if it is, just look in the internal data since we are the source
    // for it.
    if (isList) {
        MOZ_LOG(sDragLm, LogLevel::Debug, ("It's a list.."));
        uint32_t numDragItems = 0;
        // if we don't have mDataItems we didn't start this drag so it's
        // an external client trying to fool us.
        if (!mSourceDataItems)
            return NS_OK;
        mSourceDataItems->Count(&numDragItems);
        for (uint32_t itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
            nsCOMPtr<nsISupports> genericItem;
            mSourceDataItems->GetElementAt(itemIndex,
                                           getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> currItem(do_QueryInterface(genericItem));
            if (currItem) {
                nsCOMPtr <nsISupportsArray> flavorList;
                currItem->FlavorsTransferableCanExport(
                          getter_AddRefs(flavorList));
                if (flavorList) {
                    uint32_t numFlavors;
                    flavorList->Count( &numFlavors );
                    for ( uint32_t flavorIndex = 0;
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
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("checking %s against %s\n",
                                   (const char *)flavorStr, aDataFlavor));
                            if (strcmp(flavorStr, aDataFlavor) == 0) {
                                MOZ_LOG(sDragLm, LogLevel::Debug,
                                       ("boioioioiooioioioing!\n"));
                                *_retval = true;
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
    for (tmp = gdk_drag_context_list_targets(mTargetDragContext); 
         tmp; tmp = tmp->next) {
        /* Bug 331198 */
        GdkAtom atom = GDK_POINTER_TO_ATOM(tmp->data);
        gchar *name = nullptr;
        name = gdk_atom_name(atom);
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("checking %s against %s\n", name, aDataFlavor));
        if (name && (strcmp(name, aDataFlavor) == 0)) {
            MOZ_LOG(sDragLm, LogLevel::Debug, ("good!\n"));
            *_retval = true;
        }
        // check for automatic text/uri-list -> text/x-moz-url mapping
        if (!*_retval && 
            name &&
            (strcmp(name, gTextUriListType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0 ||
             strcmp(aDataFlavor, kFileMime) == 0)) {
            MOZ_LOG(sDragLm, LogLevel::Debug,
                   ("good! ( it's text/uri-list and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = true;
        }
        // check for automatic _NETSCAPE_URL -> text/x-moz-url mapping
        if (!*_retval && 
            name &&
            (strcmp(name, gMozUrlType) == 0) &&
            (strcmp(aDataFlavor, kURLMime) == 0)) {
            MOZ_LOG(sDragLm, LogLevel::Debug,
                   ("good! ( it's _NETSCAPE_URL and \
                   we're checking against text/x-moz-url )\n"));
            *_retval = true;
        }
        // check for auto text/plain -> text/unicode mapping
        if (!*_retval && 
            name &&
            (strcmp(name, kTextMime) == 0) &&
            ((strcmp(aDataFlavor, kUnicodeMime) == 0) ||
             (strcmp(aDataFlavor, kFileMime) == 0))) {
            MOZ_LOG(sDragLm, LogLevel::Debug,
                   ("good! ( it's text plain and we're checking \
                   against text/unicode or application/x-moz-file)\n"));
            *_retval = true;
        }
        g_free(name);
    }
    return NS_OK;
}

void
nsDragService::ReplyToDragMotion(GdkDragContext* aDragContext)
{
    MOZ_LOG(sDragLm, LogLevel::Debug,
           ("nsDragService::ReplyToDragMotion %d", mCanDrop));

    GdkDragAction action = (GdkDragAction)0;
    if (mCanDrop) {
        // notify the dragger if we can drop
        switch (mDragAction) {
        case DRAGDROP_ACTION_COPY:
          action = GDK_ACTION_COPY;
          break;
        case DRAGDROP_ACTION_LINK:
          action = GDK_ACTION_LINK;
          break;
        case DRAGDROP_ACTION_NONE:
          action = (GdkDragAction)0;
          break;
        default:
          action = GDK_ACTION_MOVE;
          break;
        }
    }

    gdk_drag_status(aDragContext, action, mTargetTime);
}

void
nsDragService::TargetDataReceived(GtkWidget         *aWidget,
                                  GdkDragContext    *aContext,
                                  gint               aX,
                                  gint               aY,
                                  GtkSelectionData  *aSelectionData,
                                  guint              aInfo,
                                  guint32            aTime)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::TargetDataReceived"));
    TargetResetData();
    mTargetDragDataReceived = true;
    gint len = gtk_selection_data_get_length(aSelectionData);
    const guchar* data = gtk_selection_data_get_data(aSelectionData);
    if (len > 0 && data) {
        mTargetDragDataLen = len;
        mTargetDragData = g_malloc(mTargetDragDataLen);
        memcpy(mTargetDragData, data, mTargetDragDataLen);
    }
    else {
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("Failed to get data.  selection data len was %d\n",
                mTargetDragDataLen));
    }
}

bool
nsDragService::IsTargetContextList(void)
{
    bool retval = false;

    // gMimeListType drags only work for drags within a single process. The
    // gtk_drag_get_source_widget() function will return nullptr if the source
    // of the drag is another app, so we use it to check if a gMimeListType
    // drop will work or not.
    if (gtk_drag_get_source_widget(mTargetDragContext) == nullptr)
        return retval;

    GList *tmp;

    // walk the list of context targets and see if one of them is a list
    // of items.
    for (tmp = gdk_drag_context_list_targets(mTargetDragContext); 
         tmp; tmp = tmp->next) {
        /* Bug 331198 */
        GdkAtom atom = GDK_POINTER_TO_ATOM(tmp->data);
        gchar *name = nullptr;
        name = gdk_atom_name(atom);
        if (name && strcmp(name, gMimeListType) == 0)
            retval = true;
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
    MOZ_LOG(sDragLm, LogLevel::Debug, ("getting data flavor %d\n", aFlavor));
    MOZ_LOG(sDragLm, LogLevel::Debug, ("mLastWidget is %p and mLastContext is %p\n",
                                   mTargetWidget.get(),
                                   mTargetDragContext.get()));
    // reset our target data areas
    TargetResetData();
    gtk_drag_get_data(mTargetWidget, mTargetDragContext, aFlavor, mTargetTime);
    
    MOZ_LOG(sDragLm, LogLevel::Debug, ("about to start inner iteration."));
    PRTime entryTime = PR_Now();
    while (!mTargetDragDataReceived && mDoingDrag) {
        // check the number of iterations
        MOZ_LOG(sDragLm, LogLevel::Debug, ("doing iteration...\n"));
        PR_Sleep(20*PR_TicksPerSecond()/1000);  /* sleep for 20 ms/iteration */
        if (PR_Now()-entryTime > NS_DND_TIMEOUT) break;
        gtk_main_iteration();
    }
    MOZ_LOG(sDragLm, LogLevel::Debug, ("finished inner iteration\n"));
}

void
nsDragService::TargetResetData(void)
{
    mTargetDragDataReceived = false;
    // make sure to free old data if we have to
    g_free(mTargetDragData);
    mTargetDragData = 0;
    mTargetDragDataLen = 0;
}

GtkTargetList *
nsDragService::GetSourceList(void)
{
    if (!mSourceDataItems)
        return nullptr;
    nsTArray<GtkTargetEntry*> targetArray;
    GtkTargetEntry *targets;
    GtkTargetList  *targetList = 0;
    uint32_t targetCount = 0;
    unsigned int numDragItems = 0;

    mSourceDataItems->Count(&numDragItems);

    // Check to see if we're dragging > 1 item.
    if (numDragItems > 1) {
        // as the Xdnd protocol only supports a single item (or is it just
        // gtk's implementation?), we don't advertise all flavours listed
        // in the nsITransferable.

        // the application/x-moz-internal-item-list format, which preserves
        // all information for drags within the same mozilla instance.
        GtkTargetEntry *listTarget =
            (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
        listTarget->target = g_strdup(gMimeListType);
        listTarget->flags = 0;
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("automatically adding target %s\n", listTarget->target));
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
                uint32_t numFlavors;
                flavorList->Count( &numFlavors );
                for (uint32_t flavorIndex = 0;
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
                            listTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            listTarget->target = g_strdup(gTextUriListType);
                            listTarget->flags = 0;
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("automatically adding target %s\n",
                                    listTarget->target));
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
                uint32_t numFlavors;
                flavorList->Count( &numFlavors );
                for (uint32_t flavorIndex = 0;
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
                        GtkTargetEntry *target =
                          (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                        target->target = g_strdup(flavorStr);
                        target->flags = 0;
                        MOZ_LOG(sDragLm, LogLevel::Debug,
                               ("adding target %s\n", target->target));
                        targetArray.AppendElement(target);
                        // Check to see if this is text/unicode.
                        // If it is, add text/plain
                        // since we automatically support text/plain
                        // if we support text/unicode.
                        if (strcmp(flavorStr, kUnicodeMime) == 0) {
                            GtkTargetEntry *plainUTF8Target =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            plainUTF8Target->target = g_strdup(gTextPlainUTF8Type);
                            plainUTF8Target->flags = 0;
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("automatically adding target %s\n",
                                    plainUTF8Target->target));
                            targetArray.AppendElement(plainUTF8Target);

                            GtkTargetEntry *plainTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            plainTarget->target = g_strdup(kTextMime);
                            plainTarget->flags = 0;
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("automatically adding target %s\n",
                                    plainTarget->target));
                            targetArray.AppendElement(plainTarget);
                        }
                        // Check to see if this is the x-moz-url type.
                        // If it is, add _NETSCAPE_URL
                        // this is a type used by everybody.
                        if (strcmp(flavorStr, kURLMime) == 0) {
                            GtkTargetEntry *urlTarget =
                             (GtkTargetEntry *)g_malloc(sizeof(GtkTargetEntry));
                            urlTarget->target = g_strdup(gMozUrlType);
                            urlTarget->flags = 0;
                            MOZ_LOG(sDragLm, LogLevel::Debug,
                                   ("automatically adding target %s\n",
                                    urlTarget->target));
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
        uint32_t targetIndex;
        for ( targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
            GtkTargetEntry *disEntry = targetArray.ElementAt(targetIndex);
            // this is a string reference but it will be freed later.
            targets[targetIndex].target = disEntry->target;
            targets[targetIndex].flags = disEntry->flags;
            targets[targetIndex].info = 0;
        }
        targetList = gtk_target_list_new(targets, targetCount);
        // clean up the target list
        for (uint32_t cleanIndex = 0; cleanIndex < targetCount; ++cleanIndex) {
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
    mSourceDataItems = nullptr;

    if (!mDoingDrag || mScheduledTask == eDragTaskSourceEnd)
        // EndDragSession() was already called on drop
        // or SourceEndDragSession on drag-failed
        return;

    if (mEndDragPoint.x < 0) {
        // We don't have a drag end point, so guess
        gint x, y;
        GdkDisplay* display = gdk_display_get_default();
        if (display) {
            gdk_display_get_pointer(display, nullptr, &x, &y, nullptr);
            SetDragEndPoint(nsIntPoint(x, y));
        }
    }

    // Either the drag was aborted or the drop occurred outside the app.
    // The dropEffect of mDataTransfer is not updated for motion outside the
    // app, but is needed for the dragend event, so set it now.

    uint32_t dropEffect;

    if (aResult == MOZ_GTK_DRAG_RESULT_SUCCESS) {

        // With GTK+ versions 2.10.x and prior the drag may have been
        // cancelled (but no drag-failed signal would have been sent).
        // aContext->dest_window will be non-nullptr only if the drop was
        // sent.
        GdkDragAction action =
            gdk_drag_context_get_dest_window(aContext) ? 
                gdk_drag_context_get_actions(aContext) : (GdkDragAction)0;

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
            mUserCancelled = true;
        }
    }

    if (mDataTransfer) {
        mDataTransfer->SetDropEffectInt(dropEffect);
    }

    // Schedule the appropriate drag end dom events.
    Schedule(eDragTaskSourceEnd, nullptr, nullptr, nsIntPoint(), 0);
}

static void
CreateUriList(nsISupportsArray *items, gchar **text, gint *length)
{
    uint32_t i, count;
    GString *uriList = g_string_new(nullptr);

    items->Count(&count);
    for (i = 0; i < count; i++) {
        nsCOMPtr<nsISupports> genericItem;
        items->GetElementAt(i, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> item;
        item = do_QueryInterface(genericItem);

        if (item) {
            uint32_t tmpDataLen = 0;
            void    *tmpData = nullptr;
            nsresult rv = NS_OK;
            nsCOMPtr<nsISupports> data;
            rv = item->GetTransferData(kURLMime,
                                       getter_AddRefs(data),
                                       &tmpDataLen);

            if (NS_SUCCEEDED(rv)) {
                nsPrimitiveHelpers::CreateDataFromPrimitive(kURLMime,
                                                            data,
                                                            &tmpData,
                                                            tmpDataLen);
                char* plainTextData = nullptr;
                char16_t* castedUnicode = reinterpret_cast<char16_t*>
                                                           (tmpData);
                int32_t plainTextLen = 0;
                nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText(
                                    castedUnicode,
                                    tmpDataLen / 2,
                                    &plainTextData,
                                    &plainTextLen);
                if (plainTextData) {
                    int32_t j;

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
                             guint32           aTime)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("nsDragService::SourceDataGet"));
    GdkAtom target = gtk_selection_data_get_target(aSelectionData);
    nsXPIDLCString mimeFlavor;
    gchar *typeName = 0;
    typeName = gdk_atom_name(target);
    if (!typeName) {
        MOZ_LOG(sDragLm, LogLevel::Debug, ("failed to get atom name.\n"));
        return;
    }

    MOZ_LOG(sDragLm, LogLevel::Debug, ("Type is %s\n", typeName));
    // make a copy since |nsXPIDLCString| won't use |g_free|...
    mimeFlavor.Adopt(strdup(typeName));
    g_free(typeName);
    // check to make sure that we have data items to return.
    if (!mSourceDataItems) {
        MOZ_LOG(sDragLm, LogLevel::Debug, ("Failed to get our data items\n"));
        return;
    }

    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item;
    item = do_QueryInterface(genericItem);
    if (item) {
        // if someone was asking for text/plain, lookup unicode instead so
        // we can convert it.
        bool needToDoConversionToPlainText = false;
        const char* actualFlavor = mimeFlavor;
        if (strcmp(mimeFlavor, kTextMime) == 0 ||
            strcmp(mimeFlavor, gTextPlainUTF8Type) == 0) {
            actualFlavor = kUnicodeMime;
            needToDoConversionToPlainText = true;
        }
        // if someone was asking for _NETSCAPE_URL we need to convert to
        // plain text but we also need to look for x-moz-url
        else if (strcmp(mimeFlavor, gMozUrlType) == 0) {
            actualFlavor = kURLMime;
            needToDoConversionToPlainText = true;
        }
        // if someone was asking for text/uri-list we need to convert to
        // plain text.
        else if (strcmp(mimeFlavor, gTextUriListType) == 0) {
            actualFlavor = gTextUriListType;
            needToDoConversionToPlainText = true;
        }
        else
            actualFlavor = mimeFlavor;

        uint32_t tmpDataLen = 0;
        void    *tmpData = nullptr;
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
                char* plainTextData = nullptr;
                char16_t* castedUnicode = reinterpret_cast<char16_t*>
                                                           (tmpData);
                int32_t plainTextLen = 0;
                if (strcmp(mimeFlavor, gTextPlainUTF8Type) == 0) {
                    plainTextData =
                        ToNewUTF8String(
                            nsDependentString(castedUnicode, tmpDataLen / 2),
                            (uint32_t*)&plainTextLen);
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
                gtk_selection_data_set(aSelectionData, target,
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
                gtk_selection_data_set(aSelectionData, target,
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
    RefPtr<SourceSurface> surface;
    DrawDrag(mSourceNode, mSourceRegion, mScreenX, mScreenY,
             &dragRect, &surface, &pc);
    if (!pc)
        return;

    int32_t sx = mScreenX, sy = mScreenY;
    ConvertToUnscaledDevPixels(pc, &sx, &sy);

    int32_t offsetX = sx - dragRect.x;
    int32_t offsetY = sy - dragRect.y;

    // If a popup is set as the drag image, use its widget. Otherwise, use
    // the surface that DrawDrag created.
    if (mDragPopup) {
        GtkWidget* gtkWidget = nullptr;
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
              nsImageToPixbuf::SourceSurfaceToPixbuf(surface, dragRect.width, dragRect.height);
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
    MOZ_LOG(sDragLm, LogLevel::Debug, ("invisibleSourceDragBegin"));
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
    MOZ_LOG(sDragLm, LogLevel::Debug, ("invisibleSourceDragDataGet"));
    nsDragService *dragService = (nsDragService *)aData;
    dragService->SourceDataGet(aWidget, aContext,
                               aSelectionData, aTime);
}

static gboolean
invisibleSourceDragFailed(GtkWidget        *aWidget,
                          GdkDragContext   *aContext,
                          gint              aResult,
                          gpointer          aData)
{
    MOZ_LOG(sDragLm, LogLevel::Debug, ("invisibleSourceDragFailed %i", aResult));
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
    MOZ_LOG(sDragLm, LogLevel::Debug, ("invisibleSourceDragEnd"));
    nsDragService *dragService = (nsDragService *)aData;

    // The drag has ended.  Release the hostages!
    dragService->SourceEndDragSession(aContext, MOZ_GTK_DRAG_RESULT_SUCCESS);
}

// The following methods handle responding to GTK drag signals and
// tracking state between these signals.
//
// In general, GTK does not expect us to run the event loop while handling its
// drag signals, however our drag event handlers may run the
// event loop, most often to fetch information about the drag data.
// 
// GTK, for example, uses the return value from drag-motion signals to
// determine whether drag-leave signals should be sent.  If an event loop is
// run during drag-motion the XdndLeave message can get processed but when GTK
// receives the message it does not yet know that it needs to send the
// drag-leave signal to our widget.
//
// After a drag-drop signal, we need to reply with gtk_drag_finish().
// However, gtk_drag_finish should happen after the drag-drop signal handler
// returns so that when the Motif drag protocol is used, the
// XmTRANSFER_SUCCESS during gtk_drag_finish is sent after the XmDROP_START
// reply sent on return from the drag-drop signal handler.
//
// Similarly drag-end for a successful drag and drag-failed are not good
// times to run a nested event loop as gtk_drag_drop_finished() and
// gtk_drag_source_info_destroy() don't gtk_drag_clear_source_info() or remove
// drop_timeout until after at least the first of these signals is sent.
// Processing other events (e.g. a slow GDK_DROP_FINISHED reply, or the drop
// timeout) could cause gtk_drag_drop_finished to be called again with the
// same GtkDragSourceInfo, which won't like being destroyed twice.
//
// Therefore we reply to the signals immediately and schedule a task to
// dispatch the Gecko events, which may run the event loop.
//
// Action in response to drag-leave signals is also delayed until the event
// loop runs again so that we find out whether a drag-drop signal follows.
//
// A single task is scheduled to manage responses to all three GTK signals.
// If further signals are received while the task is scheduled, the scheduled
// response is updated, sometimes effectively compressing successive signals.
//
// No Gecko drag events are dispatched (during nested event loops) while other
// Gecko drag events are in flight.  This helps event handlers that may not
// expect nested events, while accessing an event's dataTransfer for example.

gboolean
nsDragService::ScheduleMotionEvent(nsWindow *aWindow,
                                   GdkDragContext *aDragContext,
                                   nsIntPoint aWindowPoint, guint aTime)
{
    if (mScheduledTask == eDragTaskMotion) {
        // The drag source has sent another motion message before we've
        // replied to the previous.  That shouldn't happen with Xdnd.  The
        // spec for Motif drags is less clear, but we'll just update the
        // scheduled task with the new position reply only to the most
        // recent message.
        NS_WARNING("Drag Motion message received before previous reply was sent");
    }

    // Returning TRUE means we'll reply with a status message, unless we first
    // get a leave.
    return Schedule(eDragTaskMotion, aWindow, aDragContext,
                    aWindowPoint, aTime);
}

void
nsDragService::ScheduleLeaveEvent()
{
    // We don't know at this stage whether a drop signal will immediately
    // follow.  If the drop signal gets sent it will happen before we return
    // to the main loop and the scheduled leave task will be replaced.
    if (!Schedule(eDragTaskLeave, nullptr, nullptr, nsIntPoint(), 0)) {
        NS_WARNING("Drag leave after drop");
    }        
}

gboolean
nsDragService::ScheduleDropEvent(nsWindow *aWindow,
                                 GdkDragContext *aDragContext,
                                 nsIntPoint aWindowPoint, guint aTime)
{
    if (!Schedule(eDragTaskDrop, aWindow,
                  aDragContext, aWindowPoint, aTime)) {
        NS_WARNING("Additional drag drop ignored");
        return FALSE;        
    }

    SetDragEndPoint(aWindowPoint + aWindow->WidgetToScreenOffsetUntyped());

    // We'll reply with gtk_drag_finish().
    return TRUE;
}

gboolean
nsDragService::Schedule(DragTask aTask, nsWindow *aWindow,
                        GdkDragContext *aDragContext,
                        nsIntPoint aWindowPoint, guint aTime)
{
    // If there is an existing leave or motion task scheduled, then that
    // will be replaced.  When the new task is run, it will dispatch
    // any necessary leave or motion events.

    // If aTask is eDragTaskSourceEnd, then it will replace even a scheduled
    // drop event (which could happen if the drop event has not been processed
    // within the allowed time).  Otherwise, if we haven't yet run a scheduled
    // drop or end task, just say that we are not ready to receive another
    // drop.
    if (mScheduledTask == eDragTaskSourceEnd ||
        (mScheduledTask == eDragTaskDrop && aTask != eDragTaskSourceEnd))
        return FALSE;

    mScheduledTask = aTask;
    mPendingWindow = aWindow;
    mPendingDragContext = aDragContext;
    mPendingWindowPoint = aWindowPoint;
    mPendingTime = aTime;

    if (!mTaskSource) {
        // High priority is used here because the native events involved have
        // already waited at default priority.  Perhaps a lower than default
        // priority could be used for motion tasks because there is a chance
        // that a leave or drop is waiting, but managing different priorities
        // may not be worth the effort.  Motion tasks shouldn't queue up as
        // they should be throttled based on replies.
        mTaskSource = g_idle_add_full(G_PRIORITY_HIGH, TaskDispatchCallback,
                                      this, nullptr);
    }
    return TRUE;
}

gboolean
nsDragService::TaskDispatchCallback(gpointer data)
{
    nsRefPtr<nsDragService> dragService = static_cast<nsDragService*>(data);
    return dragService->RunScheduledTask();
}

gboolean
nsDragService::RunScheduledTask()
{
    if (mTargetWindow && mTargetWindow != mPendingWindow) {
        MOZ_LOG(sDragLm, LogLevel::Debug,
               ("nsDragService: dispatch drag leave (%p)\n",
                mTargetWindow.get()));
        mTargetWindow->
            DispatchDragEvent(NS_DRAGDROP_EXIT, mTargetWindowPoint, 0);

        if (!mSourceNode) {
            // The drag that was initiated in a different app. End the drag
            // session, since we're done with it for now (until the user drags
            // back into this app).
            EndDragSession(false);
        }
    }

    // It is possible that the pending state has been updated during dispatch
    // of the leave event.  That's fine.

    // Now we collect the pending state because, from this point on, we want
    // to use the same state for all events dispatched.  All state is updated
    // so that when other tasks are scheduled during dispatch here, this
    // task is considered to have already been run.
    bool positionHasChanged =
        mPendingWindow != mTargetWindow ||
        mPendingWindowPoint != mTargetWindowPoint;
    DragTask task = mScheduledTask;
    mScheduledTask = eDragTaskNone;
    mTargetWindow = mPendingWindow.forget();
    mTargetWindowPoint = mPendingWindowPoint;

    if (task == eDragTaskLeave || task == eDragTaskSourceEnd) {
        if (task == eDragTaskSourceEnd) {
            // Dispatch drag end events.
            EndDragSession(true);
        }

        // Nothing more to do
        // Returning false removes the task source from the event loop.
        mTaskSource = 0;
        return FALSE;
    }

    // This may be the start of a destination drag session.
    StartDragSession();

    // mTargetWidget may be nullptr if the window has been destroyed.
    // (The leave event is not scheduled if a drop task is still scheduled.)
    // We still reply appropriately to indicate that the drop will or didn't
    // succeeed. 
    mTargetWidget = mTargetWindow->GetMozContainerWidget();
    mTargetDragContext.steal(mPendingDragContext);
    mTargetTime = mPendingTime;

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#drag-and-drop-processing-model
    // (as at 27 December 2010) indicates that a "drop" event should only be
    // fired (at the current target element) if the current drag operation is
    // not none.  The current drag operation will only be set to a non-none
    // value during a "dragover" event.
    //
    // If the user has ended the drag before any dragover events have been
    // sent, then the spec recommends skipping the drop (because the current
    // drag operation is none).  However, here we assume that, by releasing
    // the mouse button, the user has indicated that they want to drop, so we
    // proceed with the drop where possible.
    //
    // In order to make the events appear to content in the same way as if the
    // spec is being followed we make sure to dispatch a "dragover" event with
    // appropriate coordinates and check canDrop before the "drop" event.
    //
    // When the Xdnd protocol is used for source/destination communication (as
    // should be the case with GTK source applications) a dragover event
    // should have already been sent during the drag-motion signal, which
    // would have already been received because XdndDrop messages do not
    // contain a position.  However, we can't assume the same when the Motif
    // protocol is used.
    if (task == eDragTaskMotion || positionHasChanged) {
        UpdateDragAction();
        TakeDragEventDispatchedToChildProcess(); // Clear the old value.
        DispatchMotionEvents();
        if (task == eDragTaskMotion) {
          if (TakeDragEventDispatchedToChildProcess()) {
              mTargetDragContextForRemote = mTargetDragContext;
          } else {
              // Reply to tell the source whether we can drop and what
              // action would be taken.
              ReplyToDragMotion(mTargetDragContext);
          }
        }
    }

    if (task == eDragTaskDrop) {
        gboolean success = DispatchDropEvent();

        // Perhaps we should set the del parameter to TRUE when the drag
        // action is move, but we don't know whether the data was successfully
        // transferred.
        gtk_drag_finish(mTargetDragContext, success,
                        /* del = */ FALSE, mTargetTime);

        // This drag is over, so clear out our reference to the previous
        // window.
        mTargetWindow = nullptr;
        // Make sure to end the drag session. If this drag started in a
        // different app, we won't get a drag_end signal to end it from.
        EndDragSession(true);
    }

    // We're done with the drag context.
    mTargetWidget = nullptr;
    mTargetDragContext = nullptr;

    // If we got another drag signal while running the sheduled task, that
    // must have happened while running a nested event loop.  Leave the task
    // source on the event loop.
    if (mScheduledTask != eDragTaskNone)
        return TRUE;

    // We have no task scheduled.
    // Returning false removes the task source from the event loop.
    mTaskSource = 0;
    return FALSE;
}

// This will update the drag action based on the information in the
// drag context.  Gtk gets this from a combination of the key settings
// and what the source is offering.

void
nsDragService::UpdateDragAction()
{
    // This doesn't look right.  dragSession.dragAction is used by
    // nsContentUtils::SetDataTransferInEvent() to set the initial
    // dataTransfer.dropEffect, so GdkDragContext::suggested_action would be
    // more appropriate.  GdkDragContext::actions should be used to set
    // dataTransfer.effectAllowed, which doesn't currently happen with
    // external sources.

    // default is to do nothing
    int action = nsIDragService::DRAGDROP_ACTION_NONE;
    GdkDragAction gdkAction = gdk_drag_context_get_actions(mTargetDragContext);

    // set the default just in case nothing matches below
    if (gdkAction & GDK_ACTION_DEFAULT)
        action = nsIDragService::DRAGDROP_ACTION_MOVE;

    // first check to see if move is set
    if (gdkAction & GDK_ACTION_MOVE)
        action = nsIDragService::DRAGDROP_ACTION_MOVE;

    // then fall to the others
    else if (gdkAction & GDK_ACTION_LINK)
        action = nsIDragService::DRAGDROP_ACTION_LINK;

    // copy is ctrl
    else if (gdkAction & GDK_ACTION_COPY)
        action = nsIDragService::DRAGDROP_ACTION_COPY;

    // update the drag information
    SetDragAction(action);
}

NS_IMETHODIMP
nsDragService::UpdateDragEffect()
{
  if (mTargetDragContextForRemote) {
    ReplyToDragMotion(mTargetDragContextForRemote);
    mTargetDragContextForRemote = nullptr;
  }
  return NS_OK;
}

void
nsDragService::DispatchMotionEvents()
{
    mCanDrop = false;

    FireDragEventAtSource(NS_DRAGDROP_DRAG);

    mTargetWindow->
        DispatchDragEvent(NS_DRAGDROP_OVER, mTargetWindowPoint, mTargetTime);
}

// Returns true if the drop was successful
gboolean
nsDragService::DispatchDropEvent()
{
    // We need to check IsDestroyed here because the nsRefPtr
    // only protects this from being deleted, it does NOT protect
    // against nsView::~nsView() calling Destroy() on it, bug 378273.
    if (mTargetWindow->IsDestroyed())
        return FALSE;

    EventMessage msg = mCanDrop ? eDrop : NS_DRAGDROP_EXIT;

    mTargetWindow->DispatchDragEvent(msg, mTargetWindowPoint, mTargetTime);

    return mCanDrop;
}
