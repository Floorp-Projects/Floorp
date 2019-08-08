/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.AbstractSequentialList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.NoSuchElementException;
import java.util.UUID;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.NativeQueue;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.LongDef;
import android.support.annotation.Nullable;
import android.support.annotation.NonNull;
import android.support.annotation.StringDef;
import android.support.annotation.UiThread;
import android.util.Base64;
import android.util.Log;
import android.util.LongSparseArray;
import android.view.Surface;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;

public class GeckoSession implements Parcelable {
    private static final String LOGTAG = "GeckoSession";
    private static final boolean DEBUG = false;

    // Type of changes given to onWindowChanged.
    // Window has been cleared due to the session being closed.
    private static final int WINDOW_CLOSE = 0;
    // Window has been set due to the session being opened.
    private static final int WINDOW_OPEN = 1; // Window has been opened.
    // Window has been cleared due to the session being transferred to another session.
    private static final int WINDOW_TRANSFER_OUT = 2; // Window has been transfer.
    // Window has been set due to another session being transferred to this one.
    private static final int WINDOW_TRANSFER_IN = 3;

    private enum State implements NativeQueue.State {
        INITIAL(0),
        READY(1);

        private final int mRank;

        private State(final int rank) {
            mRank = rank;
        }

        @Override
        public boolean is(final NativeQueue.State other) {
            return this == other;
        }

        @Override
        public boolean isAtLeast(final NativeQueue.State other) {
            return (other instanceof State) &&
                   mRank >= ((State) other).mRank;
        }
    }

    private final NativeQueue mNativeQueue =
        new NativeQueue(State.INITIAL, State.READY);

    private final EventDispatcher mEventDispatcher =
        new EventDispatcher(mNativeQueue);

    private final SessionTextInput mTextInput = new SessionTextInput(this, mNativeQueue);
    private SessionAccessibility mAccessibility;
    private SessionFinder mFinder;

    private String mId = UUID.randomUUID().toString().replace("-", "");
    /* package */ String getId() {
        return mId;
    }

    private boolean mShouldPinOnScreen;

    // All fields are accessed on UI thread only.
    private PanZoomController mPanZoomController = new PanZoomController(this);
    private OverscrollEdgeEffect mOverscroll;
    private DynamicToolbarAnimator mToolbar;
    private CompositorController mController;

    private boolean mAttachedCompositor;
    private boolean mCompositorReady;
    private Surface mSurface;

    // All fields of coordinates are in screen units.
    private int mLeft;
    private int mTop; // Top of the surface (including toolbar);
    private int mClientTop; // Top of the client area (i.e. excluding toolbar);
    private int mOffsetX;
    private int mOffsetY;
    private int mWidth;
    private int mHeight; // Height of the surface (including toolbar);
    private int mClientHeight; // Height of the client area (i.e. excluding toolbar);
    private int mFixedBottomOffset; // The margin for fixed elements attached to the bottom of the viewport.
    private float mViewportLeft;
    private float mViewportTop;
    private float mViewportZoom = 1.0f;

    //
    // NOTE: These values are also defined in
    // gfx/layers/ipc/UiCompositorControllerMessageTypes.h and must be kept in sync. Any
    // new AnimatorMessageType added here must also be added there.
    //
    // Sent from compositor when the static toolbar wants to hide.
    /* package */ final static int STATIC_TOOLBAR_NEEDS_UPDATE      = 0;
    // Sent from compositor when the static toolbar image has been updated and is ready to
    // animate.
    /* package */ final static int STATIC_TOOLBAR_READY             = 1;
    // Sent to compositor when the real toolbar has been hidden.
    /* package */ final static int TOOLBAR_HIDDEN                   = 2;
    // Sent to compositor when the real toolbar is visible.
    /* package */ final static int TOOLBAR_VISIBLE                  = 3;
    // Sent from compositor when the static toolbar has been made visible so the real
    // toolbar should be shown.
    /* package */ final static int TOOLBAR_SHOW                     = 4;
    // Sent from compositor after first paint
    /* package */ final static int FIRST_PAINT                      = 5;
    // Sent to compositor requesting toolbar be shown immediately
    /* package */ final static int REQUEST_SHOW_TOOLBAR_IMMEDIATELY = 6;
    // Sent to compositor requesting toolbar be shown animated
    /* package */ final static int REQUEST_SHOW_TOOLBAR_ANIMATED    = 7;
    // Sent to compositor requesting toolbar be hidden immediately
    /* package */ final static int REQUEST_HIDE_TOOLBAR_IMMEDIATELY = 8;
    // Sent to compositor requesting toolbar be hidden animated
    /* package */ final static int REQUEST_HIDE_TOOLBAR_ANIMATED    = 9;
    // Sent from compositor when a layer has been updated
    /* package */ final static int LAYERS_UPDATED                   = 10;
    // Sent to compositor when the toolbar snapshot fails.
    /* package */ final static int TOOLBAR_SNAPSHOT_FAILED          = 11;
    // Special message sent from UiCompositorControllerChild once it is open
    /* package */ final static int COMPOSITOR_CONTROLLER_OPEN       = 20;
    // Special message sent from controller to query if the compositor controller is open.
    /* package */ final static int IS_COMPOSITOR_CONTROLLER_OPEN    = 21;

    /* protected */ class Compositor extends JNIObject {
        public boolean isReady() {
            return GeckoSession.this.isCompositorReady();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorAttached() {
            GeckoSession.this.onCompositorAttached();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorDetached() {
            // Clear out any pending calls on the UI thread.
            GeckoSession.this.onCompositorDetached();
        }

        @WrapForJNI(dispatchTo = "gecko")
        @Override protected native void disposeNative();

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        public native void attachNPZC(PanZoomController.NativeProvider npzc);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        public native void onBoundsChanged(int left, int top, int width, int height);

        // Gecko thread pauses compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncPauseCompositor();

        // UI thread resumes compositor and notifies Gecko thread; does not block UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncResumeResizeCompositor(int x, int y, int width, int height, Object surface);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setMaxToolbarHeight(int height);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setFixedBottomOffset(int offset);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setPinned(boolean pinned, int reason);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void sendToolbarAnimatorMessage(int message);

        @WrapForJNI(calledFrom = "ui")
        private void recvToolbarAnimatorMessage(final int message) {
            GeckoSession.this.handleCompositorMessage(message);
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setDefaultClearColor(int color);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void requestScreenPixels(final GeckoResult<Bitmap> result);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void enableLayerUpdateNotifications(boolean enable);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void sendToolbarPixelsToCompositor(final int width, final int height,
                                                         final int[] pixels);

        // The compositor invokes this function just before compositing a frame where the
        // document is different from the document composited on the last frame. In these
        // cases, the viewport information we have in Java is no longer valid and needs to
        // be replaced with the new viewport information provided.
        @WrapForJNI(calledFrom = "ui")
        private void updateRootFrameMetrics(final float scrollX, final float scrollY,
                                            final float zoom) {
            GeckoSession.this.onMetricsChanged(scrollX, scrollY, zoom);
        }

        @WrapForJNI(calledFrom = "ui")
        private void updateOverscrollVelocity(final float x, final float y) {
            GeckoSession.this.updateOverscrollVelocity(x, y);
        }

        @WrapForJNI(calledFrom = "ui")
        private void updateOverscrollOffset(final float x, final float y) {
            GeckoSession.this.updateOverscrollOffset(x, y);
        }

        @Override
        protected void finalize() throws Throwable {
            disposeNative();
        }
    }

    /* package */ final Compositor mCompositor = new Compositor();

    @WrapForJNI(stubName = "GetCompositor", calledFrom = "ui")
    private Object getCompositorFromNative() {
        // Only used by native code.
        return mCompositorReady ? mCompositor : null;
    }

    private final GeckoSessionHandler<HistoryDelegate> mHistoryHandler =
        new GeckoSessionHandler<HistoryDelegate>(
            "GeckoViewHistory", this,
            new String[]{
                "GeckoView:OnVisited",
                "GeckoView:GetVisited",
            }
        ) {
            @Override
            public void handleMessage(final HistoryDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                if ("GeckoView:OnVisited".equals(event)) {
                    final GeckoResult<Boolean> result =
                        delegate.onVisited(GeckoSession.this, message.getString("url"),
                                           message.getString("lastVisitedURL"),
                                           message.getInt("flags"));

                    if (result == null) {
                        callback.sendSuccess(false);
                        return;
                    }

                    result.accept(
                        visited -> callback.sendSuccess(visited.booleanValue()),
                        exception -> callback.sendSuccess(false));
                } else if ("GeckoView:GetVisited".equals(event)) {
                    final String[] urls = message.getStringArray("urls");

                    final GeckoResult<boolean[]> result =
                        delegate.getVisited(GeckoSession.this, urls);

                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    result.accept(
                        visited -> callback.sendSuccess(visited),
                        exception -> callback.sendError("Failed to fetch visited statuses for URIs"));
                }
            }
        };

    private static class WebExtensionSender {
        public String webExtensionId;
        public String nativeApp;

        public WebExtensionSender(final String webExtensionId, final String nativeApp) {
            this.webExtensionId = webExtensionId;
            this.nativeApp = nativeApp;
        }

        @Override
        public boolean equals(final Object other) {
            if (!(other instanceof WebExtensionSender)) {
                return false;
            }

            WebExtensionSender o = (WebExtensionSender) other;
            return webExtensionId.equals(o.webExtensionId) &&
                    nativeApp.equals(o.nativeApp);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + (webExtensionId != null ? webExtensionId.hashCode() : 0);
            result = 31 * result + (nativeApp != null ? nativeApp.hashCode() : 0);
            return result;
        }
    }

    private final class WebExtensionListener implements BundleEventListener {
        final private HashMap<WebExtensionSender, WebExtension.MessageDelegate> mMessageDelegates;

        public WebExtensionListener() {
            mMessageDelegates = new HashMap<>();
        }

        /* package */ void registerListeners() {
            getEventDispatcher().registerUiThreadListener(this,
                    "GeckoView:WebExtension:Message",
                    "GeckoView:WebExtension:PortMessage",
                    "GeckoView:WebExtension:Connect",
                    "GeckoView:WebExtension:CloseTab",
                    null);
        }

        public void setDelegate(final WebExtension webExtension,
                                final WebExtension.MessageDelegate delegate,
                                final String nativeApp) {
            mMessageDelegates.put(new WebExtensionSender(webExtension.id, nativeApp), delegate);
        }

        public WebExtension.MessageDelegate getDelegate(final WebExtension webExtension,
                                                        final String nativeApp) {
            return mMessageDelegates.get(new WebExtensionSender(webExtension.id, nativeApp));
        }

        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if (mWindow == null) {
                return;
            }

            if ("GeckoView:WebExtension:Message".equals(event)
                    || "GeckoView:WebExtension:PortMessage".equals(event)
                    || "GeckoView:WebExtension:Connect".equals(event)) {
                mWindow.runtime.getWebExtensionDispatcher()
                        .handleMessage(event, message, callback, GeckoSession.this);
            } else if ("GeckoView:WebExtension:CloseTab".equals(event)) {
                mWindow.runtime.getWebExtensionController().closeTab(message, callback, GeckoSession.this);
            }
        }
    }

    private final WebExtensionListener mWebExtensionListener;

    /**
     * Get the message delegate for <code>nativeApp</code>.
     *
     * @param webExtension {@link WebExtension} that this delegate receives messages from.
     * @param nativeApp identifier for the native app
     * @return The {@link WebExtension.MessageDelegate} attached to the
     *         <code>nativeApp</code>.  <code>null</code> if no delegate is
     *         present.
     */
    @AnyThread
    public @Nullable WebExtension.MessageDelegate getMessageDelegate(
            final @NonNull WebExtension webExtension,
            final @NonNull String nativeApp) {
        return mWebExtensionListener.getDelegate(webExtension, nativeApp);
    }

    /**
     * Defines a message delegate for a Native App.
     *
     * If a delegate is already present, this delegate will replace the
     * existing one.
     *
     * This message delegate will be responsible for handling messaging between
     * a WebExtension content script running on the {@link GeckoSession}.
     *
     * Note: To receive messages from content scripts, the WebExtension needs
     * to explicitely allow it in {@link WebExtension#WebExtension} by setting
     * {@link WebExtension.Flags#ALLOW_CONTENT_MESSAGING}.
     *
     * @param webExtension {@link WebExtension} that this delegate receives
     *                     messages from.
     *
     * @param delegate {@link WebExtension.MessageDelegate} that will receive
     *                 messages from this session.
     * @param nativeApp which native app id this message delegate will handle
     *                  messaging for.
     * @see WebExtension#setMessageDelegate
     */
    @AnyThread
    public void setMessageDelegate(final @NonNull WebExtension webExtension,
                                   final @Nullable WebExtension.MessageDelegate delegate,
                                   final @NonNull String nativeApp) {
        mWebExtensionListener.setDelegate(webExtension, delegate, nativeApp);
    }

    private final GeckoSessionHandler<ContentDelegate> mContentHandler =
        new GeckoSessionHandler<ContentDelegate>(
            "GeckoViewContent", this,
            new String[]{
                "GeckoView:ContentCrash",
                "GeckoView:ContentKill",
                "GeckoView:ContextMenu",
                "GeckoView:DOMTitleChanged",
                "GeckoView:DOMWindowClose",
                "GeckoView:ExternalResponse",
                "GeckoView:FocusRequest",
                "GeckoView:FullScreenEnter",
                "GeckoView:FullScreenExit",
                "GeckoView:WebAppManifest",
            }
        ) {
            @Override
            public void handleMessage(final ContentDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                if ("GeckoView:ContentCrash".equals(event)) {
                    close();
                    delegate.onCrash(GeckoSession.this);
                } else if ("GeckoView:ContentKill".equals(event)) {
                    close();
                    delegate.onKill(GeckoSession.this);
                } else if ("GeckoView:ContextMenu".equals(event)) {
                    final ContentDelegate.ContextElement elem =
                        new ContentDelegate.ContextElement(
                            message.getString("baseUri"),
                            message.getString("uri"),
                            message.getString("title"),
                            message.getString("alt"),
                            message.getString("elementType"),
                            message.getString("elementSrc"));

                    delegate.onContextMenu(GeckoSession.this,
                                           message.getInt("screenX"),
                                           message.getInt("screenY"),
                                           elem);

                } else if ("GeckoView:DOMTitleChanged".equals(event)) {
                    delegate.onTitleChange(GeckoSession.this,
                                           message.getString("title"));
                } else if ("GeckoView:FocusRequest".equals(event)) {
                    delegate.onFocusRequest(GeckoSession.this);
                } else if ("GeckoView:DOMWindowClose".equals(event)) {
                    delegate.onCloseRequest(GeckoSession.this);
                } else if ("GeckoView:FullScreenEnter".equals(event)) {
                    delegate.onFullScreen(GeckoSession.this, true);
                } else if ("GeckoView:FullScreenExit".equals(event)) {
                    delegate.onFullScreen(GeckoSession.this, false);
                } else if ("GeckoView:ExternalResponse".equals(event)) {
                    delegate.onExternalResponse(GeckoSession.this, new WebResponseInfo(message));
                } else if ("GeckoView:WebAppManifest".equals(event)) {
                    final GeckoBundle manifest = message.getBundle("manifest");
                    if (manifest == null) {
                        return;
                    }

                    try {
                        delegate.onWebAppManifest(GeckoSession.this, manifest.toJSONObject());
                    } catch (JSONException e) {
                        Log.e(LOGTAG, "Failed to convert web app manifest to JSON", e);
                    }
                }
            }
        };

    private final GeckoSessionHandler<NavigationDelegate> mNavigationHandler =
        new GeckoSessionHandler<NavigationDelegate>(
            "GeckoViewNavigation", this,
            new String[]{
                "GeckoView:LocationChange",
                "GeckoView:OnLoadError",
                "GeckoView:OnLoadRequest",
                "GeckoView:OnNewSession"
            }
        ) {
            // This needs to match nsIBrowserDOMWindow.idl
            private int convertGeckoTarget(final int geckoTarget) {
                switch (geckoTarget) {
                    case 0: // OPEN_DEFAULTWINDOW
                    case 1: // OPEN_CURRENTWINDOW
                        return NavigationDelegate.TARGET_WINDOW_CURRENT;
                    default: // OPEN_NEWWINDOW, OPEN_NEWTAB, OPEN_SWITCHTAB
                        return NavigationDelegate.TARGET_WINDOW_NEW;
                }
            }

            @Override
            public void handleMessage(final NavigationDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                Log.i(LOGTAG, "handleMessage " + event + " uri=" + message.getString("uri"));
                if ("GeckoView:LocationChange".equals(event)) {
                    if (message.getBoolean("isTopLevel")) {
                        delegate.onLocationChange(GeckoSession.this,
                                                  message.getString("uri"));
                    }
                    delegate.onCanGoBack(GeckoSession.this,
                                         message.getBoolean("canGoBack"));
                    delegate.onCanGoForward(GeckoSession.this,
                                            message.getBoolean("canGoForward"));
                } else if ("GeckoView:OnLoadRequest".equals(event)) {
                    final NavigationDelegate.LoadRequest request =
                        new NavigationDelegate.LoadRequest(
                              message.getString("uri"),
                              message.getString("triggerUri"),
                              message.getInt("where"),
                              message.getInt("flags"));

                    if (!IntentUtils.isUriSafeForScheme(request.uri)) {
                        callback.sendError("Blocked unsafe intent URI");

                        delegate.onLoadError(GeckoSession.this, request.uri,
                                             new WebRequestError(WebRequestError.ERROR_MALFORMED_URI,
                                                                 WebRequestError.ERROR_CATEGORY_URI));

                        return;
                    }

                    final GeckoResult<AllowOrDeny> result =
                        delegate.onLoadRequest(GeckoSession.this, request);

                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    result.accept(value -> {
                        ThreadUtils.assertOnUiThread();
                        if (value == AllowOrDeny.ALLOW) {
                            callback.sendSuccess(false);
                        } else  if (value == AllowOrDeny.DENY) {
                            callback.sendSuccess(true);
                        } else {
                            callback.sendError("Invalid response");
                        }
                    }, exception -> callback.sendError(exception.getMessage()));
                } else if ("GeckoView:OnLoadError".equals(event)) {
                    final String uri = message.getString("uri");
                    final long errorCode = message.getLong("error");
                    final int errorModule = message.getInt("errorModule");
                    final int errorClass = message.getInt("errorClass");

                    final WebRequestError err = WebRequestError.fromGeckoError(errorCode, errorModule, errorClass);

                    final GeckoResult<String> result = delegate.onLoadError(GeckoSession.this, uri, err);
                    if (result == null) {
                        if (GeckoAppShell.isFennec()) {
                            callback.sendSuccess(null);
                        } else {
                            callback.sendError("abort");
                        }
                        return;
                    }

                    result.accept(url -> {
                        if (url == null) {
                            if (GeckoAppShell.isFennec()) {
                                callback.sendSuccess(null);
                            } else {
                                callback.sendError("abort");
                            }
                        } else {
                            callback.sendSuccess(url);
                        }
                    }, exception -> callback.sendError(exception.getMessage()));
                } else if ("GeckoView:OnNewSession".equals(event)) {
                    final String uri = message.getString("uri");
                    final GeckoResult<GeckoSession> result = delegate.onNewSession(GeckoSession.this, uri);
                    if (result == null) {
                        callback.sendSuccess(null);
                        return;
                    }

                    result.accept(session -> {
                        ThreadUtils.assertOnUiThread();
                        if (session == null) {
                            callback.sendSuccess(null);
                            return;
                        }

                        if (session.isOpen()) {
                            throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
                        }

                        if (GeckoSession.this.mWindow == null) {
                            callback.sendError("Session is not attached to a window");
                        } else {
                            session.open(GeckoSession.this.mWindow.runtime);
                            callback.sendSuccess(session.getId());
                        }
                    }, exception -> callback.sendError(exception.getMessage()));
                }
            }
        };

    private final GeckoSessionHandler<ProgressDelegate> mProgressHandler =
        new GeckoSessionHandler<ProgressDelegate>(
            "GeckoViewProgress", this,
            new String[]{
                "GeckoView:PageStart",
                "GeckoView:PageStop",
                "GeckoView:ProgressChanged",
                "GeckoView:SecurityChanged",
                "GeckoView:StateUpdated"
            }
        ) {
            @Override
            public void handleMessage(final ProgressDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                Log.i(LOGTAG, "handleMessage " + event + " uri=" + message.getString("uri"));
                if ("GeckoView:PageStart".equals(event)) {
                    delegate.onPageStart(GeckoSession.this,
                                         message.getString("uri"));
                } else if ("GeckoView:PageStop".equals(event)) {
                    delegate.onPageStop(GeckoSession.this,
                                        message.getBoolean("success"));
                } else if ("GeckoView:ProgressChanged".equals(event)) {
                    delegate.onProgressChange(GeckoSession.this,
                                              message.getInt("progress"));
                } else if ("GeckoView:SecurityChanged".equals(event)) {
                    final GeckoBundle identity = message.getBundle("identity");
                    delegate.onSecurityChange(GeckoSession.this, new ProgressDelegate.SecurityInformation(identity));
                } else if ("GeckoView:StateUpdated".equals(event)) {
                    final HistoryDelegate historyDelegate = getHistoryDelegate();
                    final GeckoBundle update = message.getBundle("data");
                    if (update != null) {
                        mStateCache.updateSessionState(update);
                        final SessionState state = new SessionState(mStateCache);
                        delegate.onSessionStateChange(GeckoSession.this, state);
                        if (historyDelegate != null && update.getBundle("historychange") != null) {
                            historyDelegate.onHistoryStateChange(GeckoSession.this, state);
                        }
                    }
                }
            }
        };

    private final GeckoSessionHandler<ScrollDelegate> mScrollHandler =
        new GeckoSessionHandler<ScrollDelegate>(
            "GeckoViewScroll", this,
            new String[]{ "GeckoView:ScrollChanged" }
        ) {
            @Override
            public void handleMessage(final ScrollDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if ("GeckoView:ScrollChanged".equals(event)) {
                    delegate.onScrollChanged(GeckoSession.this,
                                             message.getInt("scrollX"),
                                             message.getInt("scrollY"));
                }
            }
        };

    private final GeckoSessionHandler<ContentBlocking.Delegate> mContentBlockingHandler =
        new GeckoSessionHandler<ContentBlocking.Delegate>(
            "GeckoViewContentBlocking", this,
            new String[]{ "GeckoView:ContentBlocked" }
        ) {
            @Override
            public void handleMessage(final ContentBlocking.Delegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if ("GeckoView:ContentBlocked".equals(event)) {
                    delegate.onContentBlocked(GeckoSession.this,
                        ContentBlocking.BlockEvent.fromBundle(message));
                }
            }
        };

    private final GeckoSessionHandler<PermissionDelegate> mPermissionHandler =
        new GeckoSessionHandler<PermissionDelegate>(
            "GeckoViewPermission", this,
            new String[] {
                "GeckoView:AndroidPermission",
                "GeckoView:ContentPermission",
                "GeckoView:MediaPermission"
            }
        ) {
            @Override
            public void handleMessage(final PermissionDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if (delegate == null) {
                    callback.sendSuccess(/* granted */ false);
                    return;
                }
                if ("GeckoView:AndroidPermission".equals(event)) {
                    delegate.onAndroidPermissionsRequest(
                            GeckoSession.this, message.getStringArray("perms"),
                            new PermissionCallback("android", callback));
                } else if ("GeckoView:ContentPermission".equals(event)) {
                    final String typeString = message.getString("perm");
                    final int type;
                    if ("geolocation".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_GEOLOCATION;
                    } else if ("desktop-notification".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION;
                    } else if ("persistent-storage".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_PERSISTENT_STORAGE;
                    } else {
                        throw new IllegalArgumentException("Unknown permission request: " + typeString);
                    }
                    delegate.onContentPermissionRequest(
                            GeckoSession.this, message.getString("uri"),
                            type, new PermissionCallback(typeString, callback));
                } else if ("GeckoView:MediaPermission".equals(event)) {
                    GeckoBundle[] videoBundles = message.getBundleArray("video");
                    GeckoBundle[] audioBundles = message.getBundleArray("audio");
                    PermissionDelegate.MediaSource[] videos = null;
                    PermissionDelegate.MediaSource[] audios = null;

                    if (videoBundles != null) {
                        videos = new PermissionDelegate.MediaSource[videoBundles.length];
                        for (int i = 0; i < videoBundles.length; i++) {
                            videos[i] = new PermissionDelegate.MediaSource(videoBundles[i]);
                        }
                    }

                    if (audioBundles != null) {
                        audios = new PermissionDelegate.MediaSource[audioBundles.length];
                        for (int i = 0; i < audioBundles.length; i++) {
                            audios[i] = new PermissionDelegate.MediaSource(audioBundles[i]);
                        }
                    }

                    delegate.onMediaPermissionRequest(
                            GeckoSession.this, message.getString("uri"),
                            videos, audios, new PermissionCallback("media", callback));
                }
            }
        };

    private final GeckoSessionHandler<SelectionActionDelegate> mSelectionActionDelegate =
        new GeckoSessionHandler<SelectionActionDelegate>(
            "GeckoViewSelectionAction", this,
            new String[] {
                "GeckoView:HideSelectionAction",
                "GeckoView:ShowSelectionAction",
            }
        ) {
            @Override
            public void handleMessage(final SelectionActionDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                if ("GeckoView:ShowSelectionAction".equals(event)) {
                    final SelectionActionDelegate.Selection selection =
                            new SelectionActionDelegate.Selection(message);

                    final @SelectionActionDelegateAction String[] actions = message.getStringArray("actions");
                    final int seqNo = message.getInt("seqNo");
                    final GeckoResponse<String> response = new GeckoResponse<String>() {
                        @Override
                        public void respond(final String action) {
                            final GeckoBundle response = new GeckoBundle(2);
                            response.putString("id", action);
                            response.putInt("seqNo", seqNo);
                            callback.sendSuccess(response);
                        }
                    };

                    delegate.onShowActionRequest(GeckoSession.this, selection,
                                                 actions, response);

                } else if ("GeckoView:HideSelectionAction".equals(event)) {
                    final String reasonString = message.getString("reason");
                    final int reason;
                    if ("invisibleselection".equals(reasonString)) {
                        reason = SelectionActionDelegate.HIDE_REASON_INVISIBLE_SELECTION;
                    } else if ("presscaret".equals(reasonString)) {
                        reason = SelectionActionDelegate.HIDE_REASON_ACTIVE_SELECTION;
                    } else if ("scroll".equals(reasonString)) {
                        reason = SelectionActionDelegate.HIDE_REASON_ACTIVE_SCROLL;
                    } else if ("visibilitychange".equals(reasonString)) {
                        reason = SelectionActionDelegate.HIDE_REASON_NO_SELECTION;
                    } else {
                        throw new IllegalArgumentException();
                    }

                    delegate.onHideAction(GeckoSession.this, reason);
                }
            }
        };

    private LongSparseArray<MediaElement> mMediaElements = new LongSparseArray<>();
    /* package */ LongSparseArray<MediaElement> getMediaElements() {
        return mMediaElements;
    }
    private final GeckoSessionHandler<MediaDelegate> mMediaHandler =
            new GeckoSessionHandler<MediaDelegate>(
                    "GeckoViewMedia", this,
                    new String[]{
                        "GeckoView:MediaAdd",
                        "GeckoView:MediaRemove",
                        "GeckoView:MediaRemoveAll",
                        "GeckoView:MediaReadyStateChanged",
                        "GeckoView:MediaTimeChanged",
                        "GeckoView:MediaPlaybackStateChanged",
                        "GeckoView:MediaMetadataChanged",
                        "GeckoView:MediaProgress",
                        "GeckoView:MediaVolumeChanged",
                        "GeckoView:MediaRateChanged",
                        "GeckoView:MediaFullscreenChanged",
                        "GeckoView:MediaError",
                        "GeckoView:MediaRecordingStatusChanged",
                    }
            ) {
        @Override
        public void handleMessage(final MediaDelegate delegate,
                                  final String event,
                                  final GeckoBundle message,
                                  final EventCallback callback) {
            if ("GeckoView:MediaAdd".equals(event)) {
                final MediaElement element = new MediaElement(message.getLong("id"), GeckoSession.this);
                delegate.onMediaAdd(GeckoSession.this, element);
                return;
            } else if ("GeckoView:MediaRemoveAll".equals(event)) {
                for (int i = 0; i < mMediaElements.size(); i++) {
                    final long key = mMediaElements.keyAt(i);
                    delegate.onMediaRemove(GeckoSession.this, mMediaElements.get(key));
                }
                mMediaElements.clear();
                return;
            } else if ("GeckoView:MediaRecordingStatusChanged".equals(event)) {
                final GeckoBundle[] deviceBundles = message.getBundleArray("devices");
                final MediaDelegate.RecordingDevice[] devices = new MediaDelegate.RecordingDevice[deviceBundles.length];
                for (int i = 0; i < deviceBundles.length; i++) {
                    devices[i] = new MediaDelegate.RecordingDevice(deviceBundles[i]);
                }
                delegate.onRecordingStatusChanged(GeckoSession.this, devices);
                return;
            }

            final long id = message.getLong("id", 0);
            final MediaElement element = mMediaElements.get(id);
            if (element == null) {
                Log.w(LOGTAG, "MediaElement not found for '" + id + "'");
                return;
            }

            if ("GeckoView:MediaTimeChanged".equals(event)) {
                element.notifyTimeChange(message.getDouble("time"));
            } else if ("GeckoView:MediaProgress".equals(event)) {
                element.notifyLoadProgress(message);
            } else if ("GeckoView:MediaMetadataChanged".equals(event)) {
                element.notifyMetadataChange(message);
            } else if ("GeckoView:MediaReadyStateChanged".equals(event)) {
                element.notifyReadyStateChange(message.getInt("readyState"));
            } else if ("GeckoView:MediaPlaybackStateChanged".equals(event)) {
                element.notifyPlaybackStateChange(message.getString("playbackState"));
            } else if ("GeckoView:MediaVolumeChanged".equals(event)) {
                element.notifyVolumeChange(message.getDouble("volume"), message.getBoolean("muted"));
            } else if ("GeckoView:MediaRateChanged".equals(event)) {
                element.notifyPlaybackRateChange(message.getDouble("rate"));
            } else if ("GeckoView:MediaFullscreenChanged".equals(event)) {
                element.notifyFullscreenChange(message.getBoolean("fullscreen"));
            } else if ("GeckoView:MediaRemove".equals(event)) {
                delegate.onMediaRemove(GeckoSession.this, element);
                mMediaElements.remove(element.getVideoId());
            } else if ("GeckoView:MediaError".equals(event)) {
                element.notifyError(message.getInt("code"));
            } else {
                throw new UnsupportedOperationException(event + " media message not implemented");
            }
        }
    };


    /* package */ int handlersCount;

    private final GeckoSessionHandler<?>[] mSessionHandlers = new GeckoSessionHandler<?>[] {
        mContentHandler, mHistoryHandler, mMediaHandler, mNavigationHandler,
        mPermissionHandler, mProgressHandler, mScrollHandler, mSelectionActionDelegate,
        mContentBlockingHandler
    };

    private static class PermissionCallback implements
        PermissionDelegate.Callback, PermissionDelegate.MediaCallback {

        private final String mType;
        private EventCallback mCallback;

        public PermissionCallback(final String type, final EventCallback callback) {
            mType = type;
            mCallback = callback;
        }

        private void submit(final Object response) {
            if (mCallback != null) {
                mCallback.sendSuccess(response);
                mCallback = null;
            }
        }

        @Override // PermissionDelegate.Callback
        public void grant() {
            if ("media".equals(mType)) {
                throw new UnsupportedOperationException();
            }
            submit(/* response */ true);
        }

        @Override // PermissionDelegate.Callback, PermissionDelegate.MediaCallback
        public void reject() {
            submit(/* response */ false);
        }

        @Override // PermissionDelegate.MediaCallback
        public void grant(final String video, final String audio) {
            if (!"media".equals(mType)) {
                throw new UnsupportedOperationException();
            }
            final GeckoBundle response = new GeckoBundle(2);
            response.putString("video", video);
            response.putString("audio", audio);
            submit(response);
        }

        @Override // PermissionDelegate.MediaCallback
        public void grant(final PermissionDelegate.MediaSource video, final PermissionDelegate.MediaSource audio) {
            grant(video != null ? video.id : null,
                  audio != null ? audio.id : null);
        }
    }

    /**
     * Get the current user agent string for this GeckoSession.
     *
     * @return a {@link GeckoResult} containing the UserAgent string
     */
    @AnyThread
    public @NonNull GeckoResult<String> getUserAgent() {
        final CallbackResult<String> result = new CallbackResult<String>() {
            @Override
            public void sendSuccess(final Object value) {
                complete((String) value);
            }
        };
        mEventDispatcher.dispatch("GeckoView:GetUserAgent", null, result);
        return result;
    }

    /**
     * Get the default user agent for this GeckoView build.
     *
     * This method does not account for any override that might have been
     * applied to the user agent string.
     *
     * @return the default user agent string
     */
    @AnyThread
    public static @NonNull String getDefaultUserAgent() {
        return BuildConfig.USER_AGENT_GECKOVIEW_MOBILE;
    }

    /**
     * Get the current prompt delegate for this GeckoSession.
     * @return PermissionDelegate instance or null if using default delegate.
     */
    @UiThread
    public @Nullable PermissionDelegate getPermissionDelegate() {
        ThreadUtils.assertOnUiThread();
        return mPermissionHandler.getDelegate();
    }

    /**
     * Set the current permission delegate for this GeckoSession.
     * @param delegate PermissionDelegate instance or null to use the default delegate.
     */
    @UiThread
    public void setPermissionDelegate(final @Nullable PermissionDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mPermissionHandler.setDelegate(delegate, this);
    }

    private PromptDelegate mPromptDelegate;

    private final Listener mListener = new Listener();

    /* package */ static final class Window extends JNIObject implements IInterface {
        public final GeckoRuntime runtime;
        private WeakReference<GeckoSession> mOwner;
        private NativeQueue mNativeQueue;
        private Binder mBinder;

        public Window(final @NonNull GeckoRuntime runtime,
                      final @NonNull GeckoSession owner,
                      final @NonNull NativeQueue nativeQueue) {
            this.runtime = runtime;
            mOwner = new WeakReference<>(owner);
            mNativeQueue = nativeQueue;
        }

        @Override // IInterface
        public Binder asBinder() {
            if (mBinder == null) {
                mBinder = new Binder();
                mBinder.attachInterface(this, Window.class.getName());
            }
            return mBinder;
        }

        // Create a new Gecko window and assign an initial set of Java session objects to it.
        @WrapForJNI(dispatchTo = "proxy")
        public static native void open(Window instance, NativeQueue queue,
                                       Compositor compositor, EventDispatcher dispatcher,
                                       SessionAccessibility.NativeProvider sessionAccessibility,
                                       GeckoBundle initData, String id, String chromeUri,
                                       int screenId, boolean privateMode);

        @Override // JNIObject
        public void disposeNative() {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                nativeDisposeNative();
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        this, "nativeDisposeNative");
            }
        }

        @WrapForJNI(dispatchTo = "proxy", stubName = "DisposeNative")
        private native void nativeDisposeNative();

        // Force the underlying Gecko window to close and release assigned Java objects.
        public void close() {
            // Reset our queue, so we don't end up with queued calls on a disposed object.
            synchronized (this) {
                if (mNativeQueue == null) {
                    // Already closed elsewhere.
                    return;
                }
                mNativeQueue.reset(State.INITIAL);
                mNativeQueue = null;
                mOwner = null;
            }

            // Detach ourselves from the binder as well, to prevent this window from being
            // read from any parcels.
            asBinder().attachInterface(null, Window.class.getName());

            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                nativeClose();
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        this, "nativeClose");
            }
        }

        @WrapForJNI(dispatchTo = "proxy", stubName = "Close")
        private native void nativeClose();

        // Assign a new set of Java session objects to the underlying Gecko window.
        // This replaces previously assigned objects from open() or transfer() calls.
        public synchronized void transfer(final GeckoSession owner,
                                          final NativeQueue queue,
                                          final Compositor compositor,
                                          final EventDispatcher dispatcher,
                                          final SessionAccessibility.NativeProvider sessionAccessibility,
                                          final GeckoBundle initData) {
            if (mNativeQueue == null) {
                // Already closed.
                return;
            }

            final GeckoSession oldOwner = mOwner.get();
            if (oldOwner != null && owner != oldOwner) {
                oldOwner.abandonWindow();
            }

            mOwner = new WeakReference<>(owner);

            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                nativeTransfer(queue, compositor, dispatcher, sessionAccessibility, initData);
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        this, "nativeTransfer",
                        NativeQueue.class, queue,
                        Compositor.class, compositor,
                        EventDispatcher.class, dispatcher,
                        SessionAccessibility.NativeProvider.class, sessionAccessibility,
                        GeckoBundle.class, initData);
            }

            if (mNativeQueue != queue) {
                // Reset the old queue to prevent old events from affecting this window.
                // Gecko will call onReady later with the new queue if needed.
                mNativeQueue.reset(State.INITIAL);
                mNativeQueue = queue;
            }
        }

        @WrapForJNI(dispatchTo = "proxy", stubName = "Transfer")
        private native void nativeTransfer(NativeQueue queue, Compositor compositor,
                                           EventDispatcher dispatcher,
                                           SessionAccessibility.NativeProvider sessionAccessibility,
                                           GeckoBundle initData);

        @WrapForJNI(dispatchTo = "proxy")
        public native void attachEditable(IGeckoEditableParent parent);

        @WrapForJNI(dispatchTo = "proxy")
        public native void attachAccessibility(SessionAccessibility.NativeProvider sessionAccessibility);

        @WrapForJNI(calledFrom = "gecko")
        private synchronized void onReady(final @Nullable NativeQueue queue) {
            // onReady is called the first time the Gecko window is ready, with a null queue
            // argument. In this case, we simply set the current queue to ready state.
            //
            // After the initial call, onReady is called again every time Window.transfer()
            // is called, with a non-null queue argument. In this case, we only set the
            // current queue to ready state _if_ the current queue matches the given queue,
            // because if the queues don't match, we know there is another onReady call coming.

            if ((queue == null && mNativeQueue == null) ||
                (queue != null && mNativeQueue != queue)) {
                return;
            }

            if (mNativeQueue.checkAndSetState(State.INITIAL, State.READY) &&
                    queue == null) {
                Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                      " - chrome startup finished");
            }
        }

        @Override
        protected void finalize() throws Throwable {
            close();
            disposeNative();
        }
    }

    private class Listener implements BundleEventListener {
        /* package */ void registerListeners() {
            getEventDispatcher().registerUiThreadListener(this,
                "GeckoView:PinOnScreen",
                "GeckoView:Prompt",
                null);
        }

        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if (DEBUG) {
                Log.d(LOGTAG, "handleMessage: event = " + event);
            }

            if ("GeckoView:PinOnScreen".equals(event)) {
                GeckoSession.this.setShouldPinOnScreen(message.getBoolean("pinned"));
            } else if ("GeckoView:Prompt".equals(event)) {
                handlePromptEvent(GeckoSession.this, message, callback);
            }
        }
    }

    protected @Nullable Window mWindow;
    private GeckoSessionSettings mSettings;

    public GeckoSession() {
        this(null);
    }

    public GeckoSession(final @Nullable GeckoSessionSettings settings) {
        mSettings = new GeckoSessionSettings(settings, this);
        mListener.registerListeners();

        mWebExtensionListener = new WebExtensionListener();
        mWebExtensionListener.registerListeners();

        if (BuildConfig.DEBUG && handlersCount != mSessionHandlers.length) {
            throw new AssertionError("Add new handler to handlers list");
        }
    }

    /* package */ @Nullable GeckoRuntime getRuntime() {
        if (mWindow == null) {
            return null;
        }
        return mWindow.runtime;
    }

    /* package */ synchronized void abandonWindow() {
        if (mWindow == null) {
            return;
        }

        onWindowChanged(WINDOW_TRANSFER_OUT, /* inProgress */ true);
        mWindow = null;
        onWindowChanged(WINDOW_TRANSFER_OUT, /* inProgress */ false);
    }

    private void transferFrom(final Window window,
                              final GeckoSessionSettings settings,
                              final String id) {
        if (isOpen()) {
            // We will leak the existing Window if we transfer in another one.
            throw new IllegalStateException("Session is open");
        }

        if (window != null) {
            onWindowChanged(WINDOW_TRANSFER_IN, /* inProgress */ true);
        }

        mWindow = window;
        mSettings = new GeckoSessionSettings(settings, this);
        mId = id;

        if (mWindow != null) {
            mWindow.transfer(this, mNativeQueue, mCompositor,
                    mEventDispatcher, mAccessibility != null ? mAccessibility.nativeProvider : null,
                    createInitData());
            onWindowChanged(WINDOW_TRANSFER_IN, /* inProgress */ false);
        }
    }

    /* package */ void transferFrom(final GeckoSession session) {
        transferFrom(session.mWindow, session.mSettings, session.mId);
        session.mWindow = null;
    }

    @Override // Parcelable
    @AnyThread
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    @AnyThread
    public void writeToParcel(final Parcel out, final int flags) {
        out.writeStrongInterface(mWindow);
        out.writeParcelable(mSettings, flags);
        out.writeString(mId);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    @AnyThread
    public void readFromParcel(final @NonNull Parcel source) {
        final IBinder binder = source.readStrongBinder();
        final IInterface ifce = (binder != null) ?
                binder.queryLocalInterface(Window.class.getName()) : null;
        final Window window = (ifce instanceof Window) ? (Window) ifce : null;
        final GeckoSessionSettings settings =
                source.readParcelable(getClass().getClassLoader());
        final String id = source.readString();
        transferFrom(window, settings, id);
    }

    public static final Creator<GeckoSession> CREATOR = new Creator<GeckoSession>() {
        @Override
        @AnyThread
        public GeckoSession createFromParcel(final Parcel in) {
            final GeckoSession session = new GeckoSession();
            session.readFromParcel(in);
            return session;
        }

        @Override
        @AnyThread
        public GeckoSession[] newArray(final int size) {
            return new GeckoSession[size];
        }
    };

    @Override
    @AnyThread
    public int hashCode() {
        return mId.hashCode();
    }

    @Override
    @AnyThread
    public boolean equals(final Object obj) {
        return obj instanceof GeckoSession && mId.equals(((GeckoSession) obj).mId);
    }

    /**
     * Return whether this session is open.
     *
     * @return True if session is open.
     * @see #open
     * @see #close
     */
    @AnyThread
    public boolean isOpen() {
        return mWindow != null;
    }

    /* package */ boolean isReady() {
        return mNativeQueue.isReady();
    }

    private GeckoBundle createInitData() {
        final GeckoBundle initData = new GeckoBundle(2);
        initData.putBundle("settings", mSettings.toBundle());

        final GeckoBundle modules = new GeckoBundle(mSessionHandlers.length);
        for (final GeckoSessionHandler<?> handler : mSessionHandlers) {
            modules.putBoolean(handler.getName(), handler.isEnabled());
        }
        initData.putBundle("modules", modules);
        return initData;
    }

    /**
     * Opens the session.
     *
     * Call this when you are ready to use a GeckoSession instance.
     *
     * The session is in a 'closed' state when first created. Opening it creates
     * the underlying Gecko objects necessary to load a page, etc. Most GeckoSession
     * methods only take affect on an open session, and are queued until the session
     * is opened here. Opening a session is an asynchronous operation.
     *
     * @param runtime The Gecko runtime to attach this session to.
     * @see #close
     * @see #isOpen
     */
    @UiThread
    public void open(final @NonNull GeckoRuntime runtime) {
        ThreadUtils.assertOnUiThread();

        if (isOpen()) {
            // We will leak the existing Window if we open another one.
            throw new IllegalStateException("Session is open");
        }

        final String chromeUri = mSettings.getChromeUri();
        final int screenId = mSettings.getScreenId();
        final boolean isPrivate = mSettings.getUsePrivateMode();

        mWindow = new Window(runtime, this, mNativeQueue);

        onWindowChanged(WINDOW_OPEN, /* inProgress */ true);

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            Window.open(mWindow, mNativeQueue, mCompositor, mEventDispatcher,
                        mAccessibility != null ? mAccessibility.nativeProvider : null,
                        createInitData(), mId, chromeUri, screenId, isPrivate);
        } else {
            GeckoThread.queueNativeCallUntil(
                GeckoThread.State.PROFILE_READY,
                Window.class, "open",
                Window.class, mWindow,
                NativeQueue.class, mNativeQueue,
                Compositor.class, mCompositor,
                EventDispatcher.class, mEventDispatcher,
                SessionAccessibility.NativeProvider.class,
                mAccessibility != null ? mAccessibility.nativeProvider : null,
                GeckoBundle.class, createInitData(),
                String.class, mId,
                String.class, chromeUri,
                screenId, isPrivate);
        }

        onWindowChanged(WINDOW_OPEN, /* inProgress */ false);
    }

    /**
     * Closes the session.
     *
     * This frees the underlying Gecko objects and unloads the current page. The session may be
     * reopened later, but page state is not restored. Call this when you are finished using
     * a GeckoSession instance.
     *
     * @see #open
     * @see #isOpen
     */
    @UiThread
    public void close() {
        ThreadUtils.assertOnUiThread();

        if (!isOpen()) {
            Log.w(LOGTAG, "Attempted to close a GeckoSession that was already closed.");
            return;
        }

        onWindowChanged(WINDOW_CLOSE, /* inProgress */ true);

        mWindow.close();
        mWindow.disposeNative();
        mWindow = null;

        onWindowChanged(WINDOW_CLOSE, /* inProgress */ false);
    }

    private void onWindowChanged(final int change, final boolean inProgress) {
        if ((change == WINDOW_OPEN || change == WINDOW_TRANSFER_IN) && !inProgress) {
            mTextInput.onWindowChanged(mWindow);
        }
        if ((change == WINDOW_CLOSE || change == WINDOW_TRANSFER_OUT) && !inProgress) {
            mTextInput.clearAutoFill();
        }
    }

    /**
     * Get the SessionTextInput instance for this session. May be called on any thread.
     *
     * @return SessionTextInput instance.
     */
    @AnyThread
    public @NonNull SessionTextInput getTextInput() {
        // May be called on any thread.
        return mTextInput;
    }

    /**
      * Get the SessionAccessibility instance for this session.
      *
      * @return SessionAccessibility instance.
      */
    @UiThread
    public @NonNull SessionAccessibility getAccessibility() {
        ThreadUtils.assertOnUiThread();
        if (mAccessibility != null) {
            return mAccessibility;
        }

        mAccessibility = new SessionAccessibility(this);
        if (mWindow != null) {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                mWindow.attachAccessibility(mAccessibility.nativeProvider);
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        mWindow, "attachAccessibility",
                        SessionAccessibility.NativeProvider.class, mAccessibility.nativeProvider);
            }
        }
        return mAccessibility;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { LOAD_FLAGS_NONE, LOAD_FLAGS_BYPASS_CACHE, LOAD_FLAGS_BYPASS_PROXY,
                      LOAD_FLAGS_EXTERNAL, LOAD_FLAGS_ALLOW_POPUPS, LOAD_FLAGS_FORCE_ALLOW_DATA_URI, LOAD_FLAGS_REPLACE_HISTORY })
    /* package */ @interface LoadFlags {}

    // These flags follow similarly named ones in Gecko's nsIWebNavigation.idl
    // https://searchfox.org/mozilla-central/source/docshell/base/nsIWebNavigation.idl
    //
    // We do not use the same values directly in order to insulate ourselves from
    // changes in Gecko. Instead, the flags are converted in GeckoViewNavigation.jsm.

    /**
     * Default load flag, no special considerations.
     */
    public static final int LOAD_FLAGS_NONE = 0;

    /**
     * Bypass the cache.
     */
    public static final int LOAD_FLAGS_BYPASS_CACHE = 1 << 0;

    /**
     * Bypass the proxy, if one has been configured.
     */
    public static final int LOAD_FLAGS_BYPASS_PROXY = 1 << 1;

    /**
     * The load is coming from an external app. Perform additional checks.
     */
    public static final int LOAD_FLAGS_EXTERNAL = 1 << 2;

    /**
     * Popup blocking will be disabled for this load
     */
    public static final int LOAD_FLAGS_ALLOW_POPUPS = 1 << 3;

    /**
     * Bypass the URI classifier (content blocking and Safe Browsing).
     */
    public static final int LOAD_FLAGS_BYPASS_CLASSIFIER = 1 << 4;

    /**
     * Allows a top-level data: navigation to occur. E.g. view-image
     * is an explicit user action which should be allowed.
     */
    public static final int LOAD_FLAGS_FORCE_ALLOW_DATA_URI = 1 << 5;

    /**
     * This flag specifies that any existing history entry should be replaced.
     */
    public static final int LOAD_FLAGS_REPLACE_HISTORY = 1 << 6;

    /**
     * Load the given URI.
     * @param uri The URI of the resource to load.
     */
    @AnyThread
    public void loadUri(final @NonNull String uri) {
        loadUri(uri, (GeckoSession)null, LOAD_FLAGS_NONE);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     *
     * @param uri the URI to load
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @LoadFlags int flags) {
        loadUri(uri, (GeckoSession)null, flags);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     *
     * @param uri the URI to load
     * @param referrer the referrer, may be null
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @Nullable String referrer,
                        final @LoadFlags int flags) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putString("uri", uri);
        msg.putInt("flags", flags);

        if (referrer != null) {
            msg.putString("referrerUri", referrer);
        }
        mEventDispatcher.dispatch("GeckoView:LoadUri", msg);
    }

    /**
     * Load the given URI with the specified referrer and load type. This method will also do any
     * applicable checks to ensure that the specified URI is both safe and allowable
     * according to the referring GeckoSession.
     *
     * @param uri the URI to load
     * @param referrer the referring GeckoSession, may be null
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @Nullable GeckoSession referrer,
                        final @LoadFlags int flags) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putString("uri", uri);
        msg.putInt("flags", flags);

        if (referrer != null) {
            msg.putString("referrerSessionId", referrer.mId);
        }
        mEventDispatcher.dispatch("GeckoView:LoadUri", msg);
    }

    /**
     * Load the given URI.
     * @param uri The URI of the resource to load.
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri) {
        loadUri(uri, null, LOAD_FLAGS_NONE);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     * @param uri the URI to load
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri, final @LoadFlags int flags) {
        loadUri(uri.toString(), (GeckoSession)null, flags);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     * @param uri the URI to load
     * @param referrer the Uri to use as the referrer
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri, final @Nullable Uri referrer,
                        final @LoadFlags int flags) {
        loadUri(uri.toString(), referrer != null ? referrer.toString() : null, flags);
    }

    /**
     * Load the specified String data. Internally this is converted to a data URI.
     *
     * @param data a String representing the data
     * @param mimeType the mime type of the data, e.g. "text/plain". Maybe be null, in
     *                 which case the type is guessed.
     *
     */
    @AnyThread
    public void loadString(@NonNull final String data, @Nullable final String mimeType) {
        if (data == null) {
            throw new IllegalArgumentException("data cannot be null");
        }

        loadUri(createDataUri(data, mimeType), (GeckoSession)null, LOAD_FLAGS_NONE);
    }

    /**
     * Load the specified bytes. Internally this is converted to a data URI.
     *
     * @param bytes    the data to load
     * @param mimeType the mime type of the data, e.g. video/mp4. May be null, in which
     *                 case the type is guessed.
     */
    @AnyThread
    public void loadData(@NonNull final byte[] bytes, @Nullable final String mimeType) {
        if (bytes == null) {
            throw new IllegalArgumentException("data cannot be null");
        }

        loadUri(createDataUri(bytes, mimeType), (GeckoSession)null, LOAD_FLAGS_FORCE_ALLOW_DATA_URI);
    }

    /**
     * Creates a data URI of of the form "data:&lt;mime type&gt;,&lt;base64-encoded data&gt;"
     * @param bytes the bytes that should be contained in the URL
     * @param mimeType optional mime type, e.g. text/plain
     * @return a URI String
     */
    @AnyThread
    public static @NonNull String createDataUri(@NonNull final byte[] bytes,
                                                @Nullable final String mimeType) {
        return String.format("data:%s;base64,%s", mimeType != null ? mimeType : "",
                             Base64.encodeToString(bytes, Base64.NO_WRAP));
    }

    /**
     * Creates a data URI of of the form "data:&lt;mime type&gt;,&lt;base64-encoded data&gt;"
     * @param data the String data that should be contained in the URL
     * @param mimeType optional mime type, e.g. text/plain
     * @return a URI String
     */
    @AnyThread
    public static @NonNull String createDataUri(@NonNull final String data,
                                                @Nullable final String mimeType) {
        return String.format("data:%s,%s", mimeType != null ? mimeType : "", data);
    }

    /**
    * Reload the current URI.
    */
    @AnyThread
    public void reload() {
        mEventDispatcher.dispatch("GeckoView:Reload", null);
    }

    /**
    * Stop loading.
    */
    @AnyThread
    public void stop() {
        mEventDispatcher.dispatch("GeckoView:Stop", null);
    }

    /**
    * Go back in history.
    */
    @AnyThread
    public void goBack() {
        mEventDispatcher.dispatch("GeckoView:GoBack", null);
    }

    /**
    * Go forward in history.
    */
    @AnyThread
    public void goForward() {
        mEventDispatcher.dispatch("GeckoView:GoForward", null);
    }

    /**
     * Navigate to an index in browser history; the index of the currently
     * viewed page can be retrieved from an up-to-date HistoryList by
     * calling {@link HistoryDelegate.HistoryList#getCurrentIndex()}.
     *
     * @param index The index of the location in browser history you want
     *              to navigate to.
     */
    @AnyThread
    public void gotoHistoryIndex(final int index) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putInt("index", index);
        mEventDispatcher.dispatch("GeckoView:GotoHistoryIndex", msg);
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = {FINDER_FIND_BACKWARDS, FINDER_FIND_LINKS_ONLY,
                    FINDER_FIND_MATCH_CASE, FINDER_FIND_WHOLE_WORD})
    /* package */ @interface FinderFindFlags {}

    /** Go backwards when finding the next match. */
    public static final int FINDER_FIND_BACKWARDS = 1;
    /** Perform case-sensitive match; default is to perform a case-insensitive match. */
    public static final int FINDER_FIND_MATCH_CASE = 1 << 1;
    /** Must match entire words; default is to allow matching partial words. */
    public static final int FINDER_FIND_WHOLE_WORD = 1 << 2;
    /** Limit matches to links on the page. */
    public static final int FINDER_FIND_LINKS_ONLY = 1 << 3;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = {FINDER_DISPLAY_HIGHLIGHT_ALL, FINDER_DISPLAY_DIM_PAGE,
                    FINDER_DISPLAY_DRAW_LINK_OUTLINE})
    /* package */ @interface FinderDisplayFlags {}

    /** Highlight all find-in-page matches. */
    public static final int FINDER_DISPLAY_HIGHLIGHT_ALL = 1;
    /** Dim the rest of the page when showing a find-in-page match. */
    public static final int FINDER_DISPLAY_DIM_PAGE = 1 << 1;
    /** Draw outlines around matching links. */
    public static final int FINDER_DISPLAY_DRAW_LINK_OUTLINE = 1 << 2;

    /**
     * Represent the result of a find-in-page operation.
     */
    @AnyThread
    public static class FinderResult {
        /** Whether a match was found. */
        public final boolean found;
        /** Whether the search wrapped around the top or bottom of the page. */
        public final boolean wrapped;
        /** Ordinal number of the current match starting from 1, or 0 if no match. */
        public final int current;
        /** Total number of matches found so far, or -1 if unknown. */
        public final int total;
        /** Search string. */
        @NonNull public final String searchString;
        /** Flags used for the search; either 0 or a combination of {@link #FINDER_FIND_BACKWARDS
         * FINDER_FIND_*} flags. */
        @FinderFindFlags public final int flags;
        /** URI of the link, if the current match is a link, or null otherwise. */
        @Nullable public final String linkUri;
        /** Bounds of the current match in client coordinates, or null if unknown. */
        @Nullable public final RectF clientRect;

        /* package */ FinderResult(@NonNull final GeckoBundle bundle) {
            found = bundle.getBoolean("found");
            wrapped = bundle.getBoolean("wrapped");
            current = bundle.getInt("current", 0);
            total = bundle.getInt("total", -1);
            searchString = bundle.getString("searchString");
            flags = SessionFinder.getFlagsFromBundle(bundle.getBundle("flags"));
            linkUri = bundle.getString("linkURL");

            final GeckoBundle rectBundle = bundle.getBundle("clientRect");
            if (rectBundle == null) {
                clientRect = null;
            } else {
                clientRect = new RectF((float) rectBundle.getDouble("left"),
                                       (float) rectBundle.getDouble("top"),
                                       (float) rectBundle.getDouble("right"),
                                       (float) rectBundle.getDouble("bottom"));
            }
        }

        /**
         * Empty constructor for tests
         */
        protected FinderResult() {
            found = false;
            wrapped = false;
            current = 0;
            total = 0;
            flags = 0;
            searchString = "";
            linkUri = "";
            clientRect = null;
        }
    }

    /**
     * Get the SessionFinder instance for this session, to perform find-in-page operations.
     *
     * @return SessionFinder instance.
     */
    @AnyThread
    public @NonNull SessionFinder getFinder() {
        if (mFinder == null) {
            mFinder = new SessionFinder(getEventDispatcher());
        }
        return mFinder;
    }

    /**
     * Set this GeckoSession as active or inactive, which represents if the session is currently
     * visible or not. Setting a GeckoSession to inactive will significantly reduce its memory
     * footprint, but should only be done if the GeckoSession is not currently visible. Note that
     * a session can be active (i.e. visible) but not focused. When a session is set inactive,
     * it will flush the session state and trigger a `ProgressDelegate.onSessionStateChange`
     * callback.
     *
     * @param active A boolean determining whether the GeckoSession is active.
     *
     * @see #setFocused
     */
    @AnyThread
    public void setActive(final boolean active) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putBoolean("active", active);
        mEventDispatcher.dispatch("GeckoView:SetActive", msg);

        if (!active) {
            mEventDispatcher.dispatch("GeckoView:FlushSessionState", null);
        }
    }

    /**
     * Move focus to this session or away from this session. Only one session has focus at
     * a given time. Note that a session can be unfocused but still active (i.e. visible).
     *
     * @param focused True if the session should gain focus or
     *                false if the session should lose focus.
     *
     * @see #setActive
     */
    @AnyThread
    public void setFocused(final boolean focused) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putBoolean("focused", focused);
        mEventDispatcher.dispatch("GeckoView:SetFocused", msg);
    }

    /**
     * Class representing a saved session state.
     */
    @AnyThread
    public static class SessionState extends AbstractSequentialList<HistoryDelegate.HistoryItem>
                                     implements HistoryDelegate.HistoryList, Parcelable {
        private GeckoBundle mState;

        private class SessionStateItem implements HistoryDelegate.HistoryItem {
            private final GeckoBundle mItem;

            private SessionStateItem(final @NonNull GeckoBundle item) {
                mItem = item;
            }

            @Override /* HistoryItem */
            public String getUri() {
                return mItem.getString("url");
            }

            @Override /* HistoryItem */
            public String getTitle() {
                return mItem.getString("title");
            }
        }

        private class SessionStateIterator implements ListIterator<HistoryDelegate.HistoryItem> {
            private final SessionState mState;
            private int mIndex;

            private SessionStateIterator(final @NonNull SessionState state) {
                this(state, 0);
            }

            private SessionStateIterator(final @NonNull SessionState state, final int index) {
                mIndex = index;
                mState = state;
            }

            @Override /* ListIterator */
            public void add(final HistoryDelegate.HistoryItem item) {
                throw new UnsupportedOperationException();
            }

            @Override /* ListIterator */
            public boolean hasNext() {
                final GeckoBundle[] entries = mState.getHistoryEntries();

                if (entries == null) {
                    Log.w(LOGTAG, "No history entries found.");
                    return false;
                }

                if (mIndex >= mState.getHistoryEntries().length) {
                    return false;
                }
                return true;
            }

            @Override /* ListIterator */
            public boolean hasPrevious() {
                if (mIndex <= 0) {
                    return false;
                }
                return true;
            }

            @Override /* ListIterator */
            public HistoryDelegate.HistoryItem next() {
                if (hasNext()) {
                    mIndex++;
                    return new SessionStateItem(mState.getHistoryEntries()[mIndex - 1]);
                } else {
                    throw new NoSuchElementException();
                }
            }

            @Override /* ListIterator */
            public int nextIndex() {
                return mIndex;
            }

            @Override /* ListIterator */
            public HistoryDelegate.HistoryItem previous() {
                if (hasPrevious()) {
                    mIndex--;
                    return new SessionStateItem(mState.getHistoryEntries()[mIndex]);
                } else {
                    throw new NoSuchElementException();
                }
            }

            @Override /* ListIterator */
            public int previousIndex() {
                return mIndex - 1;
            }

            @Override /* ListIterator */
            public void remove() {
                throw new UnsupportedOperationException();
            }

            @Override /* ListIterator */
            public void set(final @NonNull HistoryDelegate.HistoryItem item) {
                throw new UnsupportedOperationException();
            }
        }

        private SessionState() {
            mState = new GeckoBundle(3);
        }

        private SessionState(final @NonNull GeckoBundle state) {
            mState = new GeckoBundle(state);
        }

        public SessionState(final @NonNull SessionState state) {
            mState = new GeckoBundle(state.mState);
        }

        /* package */ void updateSessionState(final @NonNull GeckoBundle updateData) {
            if (updateData == null) {
                Log.w(LOGTAG, "Session state update has no data field.");
                return;
            }

            final GeckoBundle history = updateData.getBundle("historychange");
            final GeckoBundle scroll = updateData.getBundle("scroll");
            final GeckoBundle formdata = updateData.getBundle("formdata");

            if (history != null) {
                mState.putBundle("history", history);
            }

            if (scroll != null) {
                mState.putBundle("scrolldata", scroll);
            }

            if (formdata != null) {
                mState.putBundle("formdata", formdata);
            }

            return;
        }

        @Override
        public int hashCode() {
            return mState.hashCode();
        }

        @Override
        public boolean equals(final Object other) {
            if (other == null || !(other instanceof SessionState)) {
                return false;
            }

            final SessionState otherState = (SessionState)other;

            return this.mState.equals(otherState.mState);
        }

        /**
         * Creates a new SessionState instance from a value previously returned by
         * {@link #toString()}.
         *
         * @param value The serialized SessionState in String form.
         * @return A new SessionState instance.
         * @throws JSONException if the value is not a valid json
         */
        public static @NonNull SessionState fromString(final @NonNull String value) throws JSONException {
            return new SessionState(GeckoBundle.fromJSONObject(new JSONObject(value)));
        }

        @Override
        public String toString() {
            if (mState == null) {
                Log.w(LOGTAG, "Can't convert SessionState with null state to string");
                return null;
            }

            String res;
            try {
                res = mState.toJSONObject().toString();
            } catch (JSONException e) {
                Log.e(LOGTAG, "Could not convert session state to string.");
                res = null;
            }

            return res;
        }

        @Override // Parcelable
        public int describeContents() {
            return 0;
        }

        @Override // Parcelable
        public void writeToParcel(final Parcel dest, final int flags) {
            dest.writeString(toString());
        }

        // AIDL code may call readFromParcel even though it's not part of Parcelable.
        public void readFromParcel(final @NonNull Parcel source) {
            if (source.readString() == null) {
                Log.w(LOGTAG, "Can't reproduce session state from Parcel");
            }

            try {
                mState = GeckoBundle.fromJSONObject(new JSONObject(source.readString()));
            } catch (JSONException e) {
                Log.e(LOGTAG, "Could not convert string to session state.");
                mState = null;
            }
        }

        public static final Parcelable.Creator<SessionState> CREATOR =
                new Parcelable.Creator<SessionState>() {
            @Override
            public SessionState createFromParcel(final Parcel source) {
                if (source.readString() == null) {
                    Log.w(LOGTAG, "Can't create session state from Parcel");
                }

                GeckoBundle res;
                try {
                    res = GeckoBundle.fromJSONObject(new JSONObject(source.readString()));
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Could not convert parcel to session state.");
                    res = null;
                }

                return new SessionState(res);
            }

            @Override
            public SessionState[] newArray(final int size) {
                return new SessionState[size];
            }
        };

        @Override /* AbstractSequentialList */
        public @NonNull HistoryDelegate.HistoryItem get(final int index) {
            final GeckoBundle[] entries = getHistoryEntries();

            if (entries == null || index < 0 || index >= entries.length) {
                throw new NoSuchElementException();
            }

            return new SessionStateItem(entries[index]);
        }

        @Override /* AbstractSequentialList */
        public @NonNull Iterator<HistoryDelegate.HistoryItem> iterator() {
            return listIterator(0);
        }

        @Override /* AbstractSequentialList */
        public @NonNull ListIterator<HistoryDelegate.HistoryItem> listIterator(final int index) {
            return new SessionStateIterator(this, index);
        }

        @Override /* AbstractSequentialList */
        public int size() {
            final GeckoBundle[] entries = getHistoryEntries();

            if (entries == null) {
                Log.w(LOGTAG, "No history entries found.");
                return 0;
            }

            return entries.length;
        }

        @Override /* HistoryList */
        public int getCurrentIndex() {
            final GeckoBundle history = getHistory();

            if (history == null) {
                throw new IllegalStateException("No history state exists.");
            }

            return history.getInt("index") + history.getInt("fromIdx");
        }

        // Some helpers for common code.
        private GeckoBundle getHistory() {
            if (mState == null) {
                return null;
            }

            return mState.getBundle("history");
        }

        private GeckoBundle[] getHistoryEntries() {
            final GeckoBundle history = getHistory();

            if (history == null) {
                return null;
            }

            return history.getBundleArray("entries");
        }
    }

    private SessionState mStateCache = new SessionState();

    /**
     * Restore a saved state to this GeckoSession; only data that is saved (history, scroll
     * position, zoom, and form data) will be restored. These will overwrite the corresponding
     * state of this GeckoSession.
     *
     * @param state A saved session state; this should originate from onSessionStateChange().
     */
    @AnyThread
    public void restoreState(final @NonNull SessionState state) {
        mEventDispatcher.dispatch("GeckoView:RestoreState", state.mState);
    }

    // This is the GeckoDisplay acquired via acquireDisplay(), if any.
    private GeckoDisplay mDisplay;
    /* package */ GeckoDisplay getDisplay() {
        return mDisplay;
    }

    /**
     * Acquire the GeckoDisplay instance for providing the session with a drawing Surface.
     * Be sure to call {@link GeckoDisplay#surfaceChanged(Surface, int, int)} on the
     * acquired display if there is already a valid Surface.
     *
     * @return GeckoDisplay instance.
     * @see #releaseDisplay(GeckoDisplay)
     */
    @UiThread
    public @NonNull GeckoDisplay acquireDisplay() {
        ThreadUtils.assertOnUiThread();

        if (mDisplay != null) {
            throw new IllegalStateException("Display already acquired");
        }

        mDisplay = new GeckoDisplay(this);
        return mDisplay;
    }

    /**
     * Release an acquired GeckoDisplay instance. Be sure to call {@link
     * GeckoDisplay#surfaceDestroyed()} before releasing the display if it still has a
     * valid Surface.
     *
     * @param display Acquired GeckoDisplay instance.
     * @see #acquireDisplay()
     */
    @UiThread
    public void releaseDisplay(final @NonNull GeckoDisplay display) {
        ThreadUtils.assertOnUiThread();

        if (display != mDisplay) {
            throw new IllegalArgumentException("Display not attached");
        }

        mDisplay = null;
    }

    @AnyThread
    public @NonNull GeckoSessionSettings getSettings() {
        return mSettings;
    }

    /**
    * Exits fullscreen mode
    */
    @AnyThread
    public void exitFullScreen() {
        mEventDispatcher.dispatch("GeckoViewContent:ExitFullScreen", null);
    }

    /**
    * Set the content callback handler.
    * This will replace the current handler.
    * @param delegate An implementation of ContentDelegate.
    */
    @UiThread
    public void setContentDelegate(final @Nullable ContentDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mContentHandler.setDelegate(delegate, this);
    }

    /**
    * Get the content callback handler.
    * @return The current content callback handler.
    */
    @UiThread
    public @Nullable ContentDelegate getContentDelegate() {
        ThreadUtils.assertOnUiThread();
        return mContentHandler.getDelegate();
    }

    /**
    * Set the progress callback handler.
    * This will replace the current handler.
    * @param delegate An implementation of ProgressDelegate.
    */
    @UiThread
    public void setProgressDelegate(final @Nullable ProgressDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mProgressHandler.setDelegate(delegate, this);
    }

    /**
    * Get the progress callback handler.
    * @return The current progress callback handler.
    */
    @UiThread
    public @Nullable ProgressDelegate getProgressDelegate() {
        ThreadUtils.assertOnUiThread();
        return mProgressHandler.getDelegate();
    }

    /**
    * Set the navigation callback handler.
    * This will replace the current handler.
    * @param delegate An implementation of NavigationDelegate.
    */
    @UiThread
    public void setNavigationDelegate(final @Nullable NavigationDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mNavigationHandler.setDelegate(delegate, this);
    }

    /**
    * Get the navigation callback handler.
    * @return The current navigation callback handler.
    */
    @UiThread
    public @Nullable NavigationDelegate getNavigationDelegate() {
        ThreadUtils.assertOnUiThread();
        return mNavigationHandler.getDelegate();
    }

    /**
    * Set the content scroll callback handler.
    * This will replace the current handler.
    * @param delegate An implementation of ScrollDelegate.
    */
    @UiThread
    public void setScrollDelegate(final @Nullable ScrollDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mScrollHandler.setDelegate(delegate, this);
    }

    @UiThread
    public @Nullable ScrollDelegate getScrollDelegate() {
        ThreadUtils.assertOnUiThread();
        return mScrollHandler.getDelegate();
    }

    /**
     * Set the history tracking delegate for this session, replacing the
     * current delegate if one is set.
     *
     * @param delegate The history tracking delegate, or {@code null} to unset.
     */
    @AnyThread
    public void setHistoryDelegate(final @Nullable HistoryDelegate delegate) {
        mHistoryHandler.setDelegate(delegate, this);
    }

    /** @return The history tracking delegate for this session. */
    @AnyThread
    public @Nullable HistoryDelegate getHistoryDelegate() {
        return mHistoryHandler.getDelegate();
    }

    /**
    * Set the content blocking callback handler.
    * This will replace the current handler.
    * @param delegate An implementation of {@link ContentBlocking.Delegate}.
    */
    @AnyThread
    public void setContentBlockingDelegate(final @Nullable ContentBlocking.Delegate delegate) {
        mContentBlockingHandler.setDelegate(delegate, this);
    }

    /**
    * Get the content blocking callback handler.
    * @return The current content blocking callback handler.
    */
    @AnyThread
    public @Nullable ContentBlocking.Delegate getContentBlockingDelegate() {
        return mContentBlockingHandler.getDelegate();
    }

    /**
     * Set the current prompt delegate for this GeckoSession.
     * @param delegate PromptDelegate instance or null to use the built-in delegate.
     */
    @AnyThread
    public void setPromptDelegate(final @Nullable PromptDelegate delegate) {
        mPromptDelegate = delegate;
    }

    /**
     * Get the current prompt delegate for this GeckoSession.
     * @return PromptDelegate instance or null if using built-in delegate.
     */
    @AnyThread
    public @Nullable PromptDelegate getPromptDelegate() {
        return mPromptDelegate;
    }

    /**
     * Set the current selection action delegate for this GeckoSession.
     *
     * @param delegate SelectionActionDelegate instance or null to unset.
     */
    @UiThread
    public void setSelectionActionDelegate(final @Nullable SelectionActionDelegate delegate) {
        ThreadUtils.assertOnUiThread();

        if (getSelectionActionDelegate() != null) {
            // When the delegate is changed or cleared, make sure onHideAction is called
            // one last time to hide any existing selection action UI. Gecko doesn't keep
            // track of the old delegate, so we can't rely on Gecko to do that for us.
            getSelectionActionDelegate().onHideAction(
                    this, GeckoSession.SelectionActionDelegate.HIDE_REASON_NO_SELECTION);
        }
        mSelectionActionDelegate.setDelegate(delegate, this);
    }

    /**
     * Set the media callback handler.
     * This will replace the current handler.
     * @param delegate An implementation of MediaDelegate.
     */
    @AnyThread
    public void setMediaDelegate(final @Nullable MediaDelegate delegate) {
        mMediaHandler.setDelegate(delegate, this);
    }

    /**
     * Get the Media callback handler.
     * @return The current Media callback handler.
     */
    @AnyThread
    public @Nullable MediaDelegate getMediaDelegate() {
        return mMediaHandler.getDelegate();
    }


    /**
     * Get the current selection action delegate for this GeckoSession.
     *
     * @return SelectionActionDelegate instance or null if not set.
     */
    @AnyThread
    public @Nullable SelectionActionDelegate getSelectionActionDelegate() {
        return mSelectionActionDelegate.getDelegate();
    }

    private static class PromptCallback implements
        PromptDelegate.AlertCallback, PromptDelegate.ButtonCallback,
        PromptDelegate.TextCallback, PromptDelegate.AuthCallback,
        PromptDelegate.ChoiceCallback, PromptDelegate.FileCallback {

        private final String mType;
        private final String mMode;
        private final boolean mHasCheckbox;
        private final String mCheckboxMessage;

        private EventCallback mCallback;
        private boolean mCheckboxValue;
        private GeckoBundle mResult;

        public PromptCallback(final String type, final String mode,
                              final GeckoBundle message, final EventCallback callback) {
            mType = type;
            mMode = mode;
            mCallback = callback;
            mHasCheckbox = message.getBoolean("hasCheck");
            mCheckboxMessage = message.getString("checkMsg");
            mCheckboxValue = message.getBoolean("checkValue");
        }

        private GeckoBundle ensureResult() {
            if (mResult == null) {
                // Usually result object contains two items.
                mResult = new GeckoBundle(2);
            }
            return mResult;
        }

        private void submit() {
            if (mHasCheckbox) {
                ensureResult().putBoolean("checkValue", mCheckboxValue);
            }
            if (mCallback != null) {
                mCallback.sendSuccess(mResult);
                mCallback = null;
            }
        }

        @Override // AlertCallbcak
        public void dismiss() {
            // Send a null result.
            mResult = null;
            submit();
        }

        @Override // AlertCallbcak
        public boolean hasCheckbox() {
            return mHasCheckbox;
        }

        @Override // AlertCallbcak
        public String getCheckboxMessage() {
            return mCheckboxMessage;
        }

        @Override // AlertCallbcak
        public boolean getCheckboxValue() {
            return mCheckboxValue;
        }

        @Override // AlertCallbcak
        public void setCheckboxValue(final boolean value) {
            mCheckboxValue = value;
        }

        @Override // ButtonCallback
        public void confirm(final int value) {
            if ("button".equals(mType)) {
                ensureResult().putInt("button", value);
            } else {
                throw new UnsupportedOperationException();
            }
            submit();
        }

        @Override // TextCallback, AuthCallback, ChoiceCallback, FileCallback
        public void confirm(final String value) {
            if ("text".equals(mType) || "color".equals(mType) || "datetime".equals(mType)) {
                ensureResult().putString(mType, value);
            } else if ("auth".equals(mType)) {
                if (!"password".equals(mMode)) {
                    throw new IllegalArgumentException();
                }
                ensureResult().putString("password", value);
            } else if ("choice".equals(mType)) {
                confirm(new String[] { value });
                return;
            } else {
                throw new UnsupportedOperationException();
            }
            submit();
        }

        @Override // AuthCallback
        public void confirm(final String username, final String password) {
            if ("auth".equals(mType)) {
                if (!"auth".equals(mMode)) {
                    throw new IllegalArgumentException();
                }
                ensureResult().putString("username", username);
                ensureResult().putString("password", password);
            } else {
                throw new UnsupportedOperationException();
            }
            submit();
        }

        @Override // ChoiceCallback, FileCallback
        public void confirm(final String[] values) {
            if (("menu".equals(mMode) || "single".equals(mMode)) &&
                (values == null || values.length != 1)) {
                throw new IllegalArgumentException();
            }
            if ("choice".equals(mType)) {
                ensureResult().putStringArray("choices", values);
            } else {
                throw new UnsupportedOperationException();
            }
            submit();
        }

        @Override // ChoiceCallback
        public void confirm(final PromptDelegate.Choice item) {
            if ("choice".equals(mType)) {
                confirm(item == null ? null : item.id);
                return;
            } else {
                throw new UnsupportedOperationException();
            }
        }

        @Override // ChoiceCallback
        public void confirm(final PromptDelegate.Choice[] items) {
            if (("menu".equals(mMode) || "single".equals(mMode)) &&
                (items == null || items.length != 1)) {
                throw new IllegalArgumentException();
            }
            if ("choice".equals(mType)) {
                if (items == null) {
                    confirm((String[]) null);
                    return;
                }
                final String[] ids = new String[items.length];
                for (int i = 0; i < ids.length; i++) {
                    ids[i] = (items[i] == null) ? null : items[i].id;
                }
                confirm(ids);
                return;
            } else {
                throw new UnsupportedOperationException();
            }
        }

        @Override // FileCallback
        public void confirm(final Context context, final Uri uri) {
            if ("file".equals(mType)) {
                confirm(context, uri == null ? null : new Uri[] { uri });
                return;
            } else {
                throw new UnsupportedOperationException();
            }
        }

        private static String getFile(final Context context, final Uri uri) {
            if (uri == null) {
                return null;
            }
            if ("file".equals(uri.getScheme())) {
                return uri.getPath();
            }
            final ContentResolver cr = context.getContentResolver();
            final Cursor cur = cr.query(uri, new String[] { "_data" }, /* selection */ null,
                                        /* args */ null, /* sort */ null);
            if (cur == null) {
                return null;
            }
            try {
                final int idx = cur.getColumnIndex("_data");
                if (idx < 0 || !cur.moveToFirst()) {
                    return null;
                }
                do {
                    try {
                        final String path = cur.getString(idx);
                        if (path != null && !path.isEmpty()) {
                            return path;
                        }
                    } catch (final Exception e) {
                    }
                } while (cur.moveToNext());
            } finally {
                cur.close();
            }
            return null;
        }

        @Override // FileCallback
        public void confirm(final Context context, final Uri[] uris) {
            if ("single".equals(mMode) && (uris == null || uris.length != 1)) {
                throw new IllegalArgumentException();
            }
            if ("file".equals(mType)) {
                final String[] paths = new String[uris != null ? uris.length : 0];
                for (int i = 0; i < paths.length; i++) {
                    paths[i] = getFile(context, uris[i]);
                    if (paths[i] == null) {
                        Log.e(LOGTAG, "Only file URI is supported: " + uris[i]);
                    }
                }
                ensureResult().putStringArray("files", paths);
            } else {
                throw new UnsupportedOperationException();
            }
            submit();
        }
    }

    /* package */ static void handlePromptEvent(final GeckoSession session,
                                                final GeckoBundle message,
                                                final EventCallback callback) {
        final PromptDelegate delegate = session.getPromptDelegate();
        if (delegate == null) {
            // Default behavior is same as calling dismiss() on callback.
            callback.sendSuccess(null);
            return;
        }

        final String type = message.getString("type");
        final String mode = message.getString("mode");
        final PromptCallback cb = new PromptCallback(type, mode, message, callback);
        final String title = message.getString("title");
        final String msg = message.getString("msg");
        switch (type) {
            case "alert": {
                delegate.onAlert(session, title, msg, cb);
                break;
            }
            case "button": {
                final String[] btnTitle = message.getStringArray("btnTitle");
                final String[] btnCustomTitle = message.getStringArray("btnCustomTitle");
                for (int i = 0; i < btnCustomTitle.length; i++) {
                    final int resId;
                    if ("ok".equals(btnTitle[i])) {
                        resId = android.R.string.ok;
                    } else if ("cancel".equals(btnTitle[i])) {
                        resId = android.R.string.cancel;
                    } else if ("yes".equals(btnTitle[i])) {
                        resId = android.R.string.yes;
                    } else if ("no".equals(btnTitle[i])) {
                        resId = android.R.string.no;
                    } else {
                        continue;
                    }
                    btnCustomTitle[i] = Resources.getSystem().getString(resId);
                }
                delegate.onButtonPrompt(session, title, msg, btnCustomTitle, cb);
                break;
            }
            case "text": {
                delegate.onTextPrompt(session, title, msg, message.getString("value"), cb);
                break;
            }
            case "auth": {
                delegate.onAuthPrompt(session, title, msg, new PromptDelegate.AuthOptions(message.getBundle("options")), cb);
                break;
            }
            case "choice": {
                final int intMode;
                if ("menu".equals(mode)) {
                    intMode = PromptDelegate.Choice.CHOICE_TYPE_MENU;
                } else if ("single".equals(mode)) {
                    intMode = PromptDelegate.Choice.CHOICE_TYPE_SINGLE;
                } else if ("multiple".equals(mode)) {
                    intMode = PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }

                GeckoBundle[] choiceBundles = message.getBundleArray("choices");
                PromptDelegate.Choice choices[];
                if (choiceBundles == null || choiceBundles.length == 0) {
                    choices = new PromptDelegate.Choice[0];
                } else {
                    choices = new PromptDelegate.Choice[choiceBundles.length];
                    for (int i = 0; i < choiceBundles.length; i++) {
                        choices[i] = new PromptDelegate.Choice(choiceBundles[i]);
                    }
                }
                delegate.onChoicePrompt(session, title, msg, intMode,
                                         choices, cb);
                break;
            }
            case "color": {
                delegate.onColorPrompt(session, title, message.getString("value"), cb);
                break;
            }
            case "datetime": {
                final int intMode;
                if ("date".equals(mode)) {
                    intMode = PromptDelegate.DATETIME_TYPE_DATE;
                } else if ("month".equals(mode)) {
                    intMode = PromptDelegate.DATETIME_TYPE_MONTH;
                } else if ("week".equals(mode)) {
                    intMode = PromptDelegate.DATETIME_TYPE_WEEK;
                } else if ("time".equals(mode)) {
                    intMode = PromptDelegate.DATETIME_TYPE_TIME;
                } else if ("datetime-local".equals(mode)) {
                    intMode = PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }
                delegate.onDateTimePrompt(session, title, intMode,
                                           message.getString("value"),
                                           message.getString("min"),
                                           message.getString("max"), cb);
                break;
            }
            case "file": {
                final int intMode;
                if ("single".equals(mode)) {
                    intMode = PromptDelegate.FILE_TYPE_SINGLE;
                } else if ("multiple".equals(mode)) {
                    intMode = PromptDelegate.FILE_TYPE_MULTIPLE;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }
                String[] mimeTypes = message.getStringArray("mimeTypes");
                int capture = message.getInt("capture");
                delegate.onFilePrompt(session, title, intMode, mimeTypes, capture, cb);
                break;
            }
            case "popup": {
                GeckoResult<AllowOrDeny> res = delegate.onPopupRequest(session, message.getString("targetUri"));

                if (res == null) {
                    // Keep the popup blocked if the delegate returns null
                    callback.sendSuccess(false);
                    return;
                }

                res.accept(value -> {
                    if (value == AllowOrDeny.ALLOW) {
                        callback.sendSuccess(true);
                    } else if (value == AllowOrDeny.DENY) {
                        callback.sendSuccess(false);
                    } else {
                        callback.sendError("Invalid response");
                    }
                }, exception ->
                        callback.sendError("Failed to get popup-blocking decision"));
                break;
            }
            default: {
                callback.sendError("Invalid type");
                break;
            }
        }
    }

    @UiThread
    protected void setShouldPinOnScreen(final boolean pinned) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mToolbar != null) {
            mToolbar.setPinned(pinned, DynamicToolbarAnimator.PinReason.CARET_DRAG);
        }

        mShouldPinOnScreen = pinned;
    }

    /* package */ boolean shouldPinOnScreen() {
        ThreadUtils.assertOnUiThread();
        return mShouldPinOnScreen;
    }

    @AnyThread
    /* package */ @NonNull EventDispatcher getEventDispatcher() {
        return mEventDispatcher;
    }

    public interface ProgressDelegate {
        /**
         * Class representing security information for a site.
         */
        public class SecurityInformation {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({SECURITY_MODE_UNKNOWN, SECURITY_MODE_IDENTIFIED,
                     SECURITY_MODE_VERIFIED})
            /* package */ @interface SecurityMode {}
            public static final int SECURITY_MODE_UNKNOWN = 0;
            public static final int SECURITY_MODE_IDENTIFIED = 1;
            public static final int SECURITY_MODE_VERIFIED = 2;

            @Retention(RetentionPolicy.SOURCE)
            @IntDef({CONTENT_UNKNOWN, CONTENT_BLOCKED, CONTENT_LOADED})
            /* package */ @interface ContentType {}
            public static final int CONTENT_UNKNOWN = 0;
            public static final int CONTENT_BLOCKED = 1;
            public static final int CONTENT_LOADED = 2;
            /**
             * Indicates whether or not the site is secure.
             */
            public final boolean isSecure;
            /**
             * Indicates whether or not the site is a security exception.
             */
            public final boolean isException;
            /**
             * Contains the origin of the certificate.
             */
            public final @Nullable String origin;
            /**
             * Contains the host associated with the certificate.
             */
            public final @NonNull String host;
            /**
             * Contains the human-readable name of the certificate subject.
             */
            public final @NonNull String organization;
            /**
             * Contains the full name of the certificate subject, including location.
             */
            public final @NonNull String subjectName;
            /**
             * Contains the common name of the issuing authority.
             */
            public final @NonNull String issuerCommonName;
            /**
             * Contains the full/proper name of the issuing authority.
             */
            public final @NonNull String issuerOrganization;
            /**
             * Indicates the security level of the site; possible values are SECURITY_MODE_UNKNOWN,
             * SECURITY_MODE_IDENTIFIED, and SECURITY_MODE_VERIFIED. SECURITY_MODE_IDENTIFIED
             * indicates domain validation only, while SECURITY_MODE_VERIFIED indicates extended validation.
             */
            public final @SecurityMode int securityMode;
            /**
             * Indicates the presence of passive mixed content; possible values are
             * CONTENT_UNKNOWN, CONTENT_BLOCKED, and CONTENT_LOADED.
             */
            public final @ContentType int mixedModePassive;
            /**
             * Indicates the presence of active mixed content; possible values are
             * CONTENT_UNKNOWN, CONTENT_BLOCKED, and CONTENT_LOADED.
             */
            public final @ContentType int mixedModeActive;

            /* package */ SecurityInformation(final GeckoBundle identityData) {
                final GeckoBundle mode = identityData.getBundle("mode");

                mixedModePassive = mode.getInt("mixed_display");
                mixedModeActive = mode.getInt("mixed_active");

                securityMode = mode.getInt("identity");

                isSecure = identityData.getBoolean("secure");
                isException = identityData.getBoolean("securityException");
                origin = identityData.getString("origin");
                host = identityData.getString("host");
                organization = identityData.getString("organization");
                subjectName = identityData.getString("subjectName");
                issuerCommonName = identityData.getString("issuerCommonName");
                issuerOrganization = identityData.getString("issuerOrganization");
            }

            /**
             * Empty constructor for tests
             */
            protected SecurityInformation() {
                mixedModePassive = 0;
                mixedModeActive = 0;
                securityMode = 0;
                isSecure = false;
                isException = false;
                origin = "";
                host = "";
                organization = "";
                subjectName = "";
                issuerCommonName = "";
                issuerOrganization = "";
            }
        }

        /**
        * A View has started loading content from the network.
        * @param session GeckoSession that initiated the callback.
        * @param url The resource being loaded.
        */
        @UiThread
        default void onPageStart(@NonNull GeckoSession session, @NonNull String url) {}

        /**
        * A View has finished loading content from the network.
        * @param session GeckoSession that initiated the callback.
        * @param success Whether the page loaded successfully or an error occurred.
        */
        @UiThread
        default void onPageStop(@NonNull GeckoSession session, boolean success) {}

        /**
         * Page loading has progressed.
         * @param session GeckoSession that initiated the callback.
         * @param progress Current page load progress value [0, 100].
         */
        @UiThread
        default void onProgressChange(@NonNull GeckoSession session, int progress) {}

        /**
        * The security status has been updated.
        * @param session GeckoSession that initiated the callback.
        * @param securityInfo The new security information.
        */
        @UiThread
        default void onSecurityChange(@NonNull GeckoSession session,
                                      @NonNull SecurityInformation securityInfo) {}

        /**
        * The browser session state has changed. This can happen in response to
        * navigation, scrolling, or form data changes; the session state passed
        * includes the most up to date information on all of these.
        * @param session GeckoSession that initiated the callback.
        * @param sessionState SessionState representing the latest browser state.
        */
        @UiThread
        default void onSessionStateChange(@NonNull GeckoSession session,
                                          @NonNull SessionState sessionState) {}
    }

    /**
     * WebResponseInfo contains information about a single web response.
     */
    @AnyThread
    static public class WebResponseInfo {
        /**
         * The URI of the response. Cannot be null.
         */
        @NonNull public final String uri;

        /**
         * The content type (mime type) of the response. May be null.
         */
        @Nullable public final String contentType;

        /**
         * The content length of the response. May be 0 if unknokwn.
         */
        @Nullable public final long contentLength;

        /**
         * The filename obtained from the content disposition, if any.
         * May be null.
         */
        @Nullable public final String filename;

        /* package */ WebResponseInfo(final GeckoBundle message) {
            uri = message.getString("uri");
            if (uri == null) {
                throw new IllegalArgumentException("URI cannot be null");
            }

            contentType = message.getString("contentType");
            contentLength = message.getLong("contentLength");
            filename = message.getString("filename");
        }

        /**
         * Empty constructor for tests.
         */
        protected WebResponseInfo() {
            uri = "";
            contentType = "";
            contentLength = 0;
            filename = "";
        }
    }

    public interface ContentDelegate {
        /**
        * A page title was discovered in the content or updated after the content
        * loaded.
        * @param session The GeckoSession that initiated the callback.
        * @param title The title sent from the content.
        */
        @UiThread
        default void onTitleChange(@NonNull GeckoSession session, @Nullable String title) {}

        /**
        * A page has requested focus. Note that window.focus() in content will not result
        * in this being called.
        * @param session The GeckoSession that initiated the callback.
        */
        @UiThread
        default void onFocusRequest(@NonNull GeckoSession session) {}

        /**
        * A page has requested to close
        * @param session The GeckoSession that initiated the callback.
        */
        @UiThread
        default void onCloseRequest(@NonNull GeckoSession session) {}

        /**
         * A page has entered or exited full screen mode. Typically, the implementation
         * would set the Activity containing the GeckoSession to full screen when the page is
         * in full screen mode.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param fullScreen True if the page is in full screen mode.
         */
        @UiThread
        default void onFullScreen(@NonNull GeckoSession session, boolean fullScreen) {}

        /**
         * Element details for onContextMenu callbacks.
         */
        public static class ContextElement {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({TYPE_NONE, TYPE_IMAGE, TYPE_VIDEO, TYPE_AUDIO})
            /* package */ @interface Type {}
            public static final int TYPE_NONE = 0;
            public static final int TYPE_IMAGE = 1;
            public static final int TYPE_VIDEO = 2;
            public static final int TYPE_AUDIO = 3;

            /**
             * The base URI of the element's document.
             */
            public final @Nullable String baseUri;

            /**
             * The absolute link URI (href) of the element.
             */
            public final @Nullable String linkUri;

            /**
             * The title text of the element.
             */
            public final @Nullable String title;

            /**
             * The alternative text (alt) for the element.
             */
            public final @Nullable String altText;

            /**
             * The type of the element.
             * One of the {@link ContextElement#TYPE_NONE} flags.
             */
            public final @Type int type;

            /**
             * The source URI (src) of the element.
             * Set for (nested) media elements.
             */
            public final @Nullable String srcUri;

            protected ContextElement(
                    final @Nullable String baseUri,
                    final @Nullable String linkUri,
                    final @Nullable String title,
                    final @Nullable String altText,
                    final @NonNull String typeStr,
                    final @Nullable String srcUri) {
                this.baseUri = baseUri;
                this.linkUri = linkUri;
                this.title = title;
                this.altText = altText;
                this.type = getType(typeStr);
                this.srcUri = srcUri;
            }

            private static int getType(final String name) {
                if ("HTMLImageElement".equals(name)) {
                    return TYPE_IMAGE;
                } else if ("HTMLVideoElement".equals(name)) {
                    return TYPE_VIDEO;
                } else if ("HTMLAudioElement".equals(name)) {
                    return TYPE_AUDIO;
                }
                return TYPE_NONE;
            }
        }

        /**
         * A user has initiated the context menu via long-press.
         * This event is fired on links, (nested) images and (nested) media
         * elements.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param screenX The screen coordinates of the press.
         * @param screenY The screen coordinates of the press.
         * @param element The details for the pressed element.
         */
        @UiThread
        default void onContextMenu(@NonNull GeckoSession session,
                                   int screenX, int screenY,
                                   @NonNull ContextElement element) {}

        /**
         * This is fired when there is a response that cannot be handled
         * by Gecko (e.g., a download).
         *
         * @param session the GeckoSession that received the external response.
         * @param response the WebResponseInfo for the external response
         */
        @UiThread
        default void onExternalResponse(@NonNull GeckoSession session,
                                        @NonNull WebResponseInfo response) {}

        /**
         * The content process hosting this GeckoSession has crashed. The
         * GeckoSession is now closed and unusable. You may call
         * {@link #open(GeckoRuntime)} to recover the session, but no state
         * is preserved. Most applications will want to call
         * {@link #loadUri(Uri)} or {@link #restoreState(SessionState)} at this point.
         *
         * @param session The GeckoSession for which the content process has crashed.
         */
        @UiThread
        default void onCrash(@NonNull GeckoSession session) {}

        /**
         * The content process hosting this GeckoSession has been killed. The
         * GeckoSession is now closed and unusable. You may call
         * {@link #open(GeckoRuntime)} to recover the session, but no state
         * is preserved. Most applications will want to call
         * {@link #loadUri(Uri)} or {@link #restoreState(SessionState)} at this point.
         *
         * @param session The GeckoSession for which the content process has been killed.
         */
        @UiThread
        default void onKill(@NonNull GeckoSession session) {}


        /**
         * Notification that the first content composition has occurred.
         * This callback is invoked for the first content composite after either
         * a start or a restart of the compositor.
         * @param session The GeckoSession that had a first paint event.
         */
        @UiThread
        default void onFirstComposite(@NonNull GeckoSession session) {}

        /**
         * This is fired when the loaded document has a valid Web App Manifest present.
         *
         * @param session The GeckoSession that contains the Web App Manifest
         * @param manifest A parsed and validated {@link JSONObject} containing the manifest contents.
         * @see <a href="https://www.w3.org/TR/appmanifest/">Web App Manifest specification</a>
         */
        @UiThread
        default void onWebAppManifest(@NonNull GeckoSession session, @NonNull JSONObject manifest) {}
    }

    public interface SelectionActionDelegate {
        /**
         * The selection is collapsed at a single position.
         */
        final int FLAG_IS_COLLAPSED = 1;
        /**
         * The selection is inside editable content such as an input element or
         * contentEditable node.
         */
        final int FLAG_IS_EDITABLE = 2;
        /**
         * The selection is inside a password field.
         */
        final int FLAG_IS_PASSWORD = 4;

        /**
         * Hide selection actions and cause {@link #onHideAction} to be called.
         */
        final String ACTION_HIDE = "org.mozilla.geckoview.HIDE";
        /**
         * Copy onto the clipboard then delete the selected content. Selection
         * must be editable.
         */
        final String ACTION_CUT = "org.mozilla.geckoview.CUT";
        /**
         * Copy the selected content onto the clipboard.
         */
        final String ACTION_COPY = "org.mozilla.geckoview.COPY";
        /**
         * Delete the selected content. Selection must be editable.
         */
        final String ACTION_DELETE = "org.mozilla.geckoview.DELETE";
        /**
         * Replace the selected content with the clipboard content. Selection
         * must be editable.
         */
        final String ACTION_PASTE = "org.mozilla.geckoview.PASTE";
        /**
         * Select the entire content of the document or editor.
         */
        final String ACTION_SELECT_ALL = "org.mozilla.geckoview.SELECT_ALL";
        /**
         * Clear the current selection. Selection must not be editable.
         */
        final String ACTION_UNSELECT = "org.mozilla.geckoview.UNSELECT";
        /**
         * Collapse the current selection to its start position.
         * Selection must be editable.
         */
        final String ACTION_COLLAPSE_TO_START = "org.mozilla.geckoview.COLLAPSE_TO_START";
        /**
         * Collapse the current selection to its end position.
         * Selection must be editable.
         */
        final String ACTION_COLLAPSE_TO_END = "org.mozilla.geckoview.COLLAPSE_TO_END";

        /**
         * Represents attributes of a selection.
         */
        class Selection {
            /**
             * Flags describing the current selection, as a bitwise combination
             * of the {@link #FLAG_IS_COLLAPSED FLAG_*} constants.
             */
            public final @SelectionActionDelegateFlag int flags;

            /**
             * Text content of the current selection. An empty string indicates the selection
             * is collapsed or the selection cannot be represented as plain text.
             */
            public final @NonNull String text;

            /**
             * The bounds of the current selection in client coordinates. Use {@link
             * GeckoSession#getClientToScreenMatrix} to perform transformation to screen
             * coordinates.
             */
            public final @Nullable RectF clientRect;

            /* package */ Selection(final GeckoBundle bundle) {
                flags = (bundle.getBoolean("collapsed") ?
                         SelectionActionDelegate.FLAG_IS_COLLAPSED : 0) |
                        (bundle.getBoolean("editable") ?
                         SelectionActionDelegate.FLAG_IS_EDITABLE : 0) |
                        (bundle.getBoolean("password") ?
                         SelectionActionDelegate.FLAG_IS_PASSWORD : 0);
                text = bundle.getString("selection");

                final GeckoBundle rectBundle = bundle.getBundle("clientRect");
                if (rectBundle == null) {
                    clientRect = null;
                } else {
                    clientRect = new RectF((float) rectBundle.getDouble("left"),
                                           (float) rectBundle.getDouble("top"),
                                           (float) rectBundle.getDouble("right"),
                                           (float) rectBundle.getDouble("bottom"));
                }
            }

            /**
             * Empty constructor for tests.
             */
            protected Selection() {
                flags = 0;
                text = "";
                clientRect = null;
            }
        }

        /**
         * Selection actions are available. Selection actions become available when the
         * user selects some content in the document or editor. Inside an editor,
         * selection actions can also become available when the user explicitly requests
         * editor action UI, for example by tapping on the caret handle.
         *
         * In response to this callback, applications typically display a toolbar
         * containing the selection actions. To perform a certain action, pass the Action
         * object back through the response parameter, which may be used multiple times to
         * perform multiple actions at once.
         *
         * Once a {@link #onHideAction} call (with particular reasons) or another {@link
         * #onShowActionRequest} call is received, any previously received actions are no
         * longer unavailable.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param selection Current selection attributes.
         * @param actions Array of built-in actions available; possible values
         * come from the {@link #ACTION_HIDE ACTION_*} constants.
         * @param response Callback object for performing built-in actions. For example,
         * {@code response.respond(actions[0])} performs the first action. May be used
         * multiple times to perform multiple actions at once.
         */
        @UiThread
        default void onShowActionRequest(@NonNull GeckoSession session, @NonNull Selection selection,
                                         @NonNull @SelectionActionDelegateAction String[] actions,
                                         @NonNull GeckoResponse<String> response) {}

        /**
         * Actions are no longer available due to the user clearing the selection.
         */
        final int HIDE_REASON_NO_SELECTION = 0;
        /**
         * Actions are no longer available due to the user moving the selection out of view.
         * Previous actions are still available after a callback with this reason.
         */
        final int HIDE_REASON_INVISIBLE_SELECTION = 1;
        /**
         * Actions are no longer available due to the user actively changing the
         * selection. {@link #onShowActionRequest} may be called again once the user has
         * set a selection, if the new selection has available actions.
         */
        final int HIDE_REASON_ACTIVE_SELECTION = 2;
        /**
         * Actions are no longer available due to the user actively scrolling the page.
         * {@link #onShowActionRequest} may be called again once the user has stopped
         * scrolling the page, if the selection is still visible. Until then, previous
         * actions are still available after a callback with this reason.
         */
        final int HIDE_REASON_ACTIVE_SCROLL = 3;

        /**
         * Previous actions are no longer available due to the user interacting with the
         * page. Applications typically hide the action toolbar in response.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param reason The reason that actions are no longer available, as one of the
         * {@link #HIDE_REASON_NO_SELECTION HIDE_REASON_*} constants.
         */
        @UiThread
        default void onHideAction(@NonNull GeckoSession session,
                                  @SelectionActionDelegateHideReason int reason) {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @StringDef({
            SelectionActionDelegate.ACTION_HIDE,
            SelectionActionDelegate.ACTION_CUT,
            SelectionActionDelegate.ACTION_COPY,
            SelectionActionDelegate.ACTION_DELETE,
            SelectionActionDelegate.ACTION_PASTE,
            SelectionActionDelegate.ACTION_SELECT_ALL,
            SelectionActionDelegate.ACTION_UNSELECT,
            SelectionActionDelegate.ACTION_COLLAPSE_TO_START,
            SelectionActionDelegate.ACTION_COLLAPSE_TO_END})
    /* package */ @interface SelectionActionDelegateAction {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true, value = {
            SelectionActionDelegate.FLAG_IS_COLLAPSED,
            SelectionActionDelegate.FLAG_IS_EDITABLE})
            /* package */ @interface SelectionActionDelegateFlag {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            SelectionActionDelegate.HIDE_REASON_NO_SELECTION,
            SelectionActionDelegate.HIDE_REASON_INVISIBLE_SELECTION,
            SelectionActionDelegate.HIDE_REASON_ACTIVE_SELECTION,
            SelectionActionDelegate.HIDE_REASON_ACTIVE_SCROLL})
    /* package */ @interface SelectionActionDelegateHideReason {}

    public interface NavigationDelegate {
        /**
        * A view has started loading content from the network.
        * @param session The GeckoSession that initiated the callback.
        * @param url The resource being loaded.
        */
        @UiThread
        default void onLocationChange(@NonNull GeckoSession session, @Nullable String url) {}

        /**
        * The view's ability to go back has changed.
        * @param session The GeckoSession that initiated the callback.
        * @param canGoBack The new value for the ability.
        */
        @UiThread
        default void onCanGoBack(@NonNull GeckoSession session, boolean canGoBack) {}

        /**
        * The view's ability to go forward has changed.
        * @param session The GeckoSession that initiated the callback.
        * @param canGoForward The new value for the ability.
        */
        @UiThread
        default void onCanGoForward(@NonNull GeckoSession session, boolean canGoForward) {}

        public static final int TARGET_WINDOW_NONE = 0;
        public static final int TARGET_WINDOW_CURRENT = 1;
        public static final int TARGET_WINDOW_NEW = 2;

        // Match with nsIWebNavigation.idl.
        /**
         * The load request was triggered by an HTTP redirect.
         */
        static final int LOAD_REQUEST_IS_REDIRECT = 0x800000;

        /**
         * Load request details.
         */
        public static class LoadRequest {
            /* package */ LoadRequest(@NonNull final String uri,
                                      @Nullable final String triggerUri,
                                      final int geckoTarget,
                                      final int flags) {
                this.uri = uri;
                this.triggerUri = triggerUri;
                this.target = convertGeckoTarget(geckoTarget);
                this.isRedirect = (flags & LOAD_REQUEST_IS_REDIRECT) != 0;
            }

            /**
             * Empty constructor for tests.
             */
            protected LoadRequest() {
                uri = "";
                triggerUri = null;
                target = 0;
                isRedirect = false;
            }

            // This needs to match nsIBrowserDOMWindow.idl
            private @TargetWindow int convertGeckoTarget(final int geckoTarget) {
                switch (geckoTarget) {
                    case 0: // OPEN_DEFAULTWINDOW
                    case 1: // OPEN_CURRENTWINDOW
                        return TARGET_WINDOW_CURRENT;
                    default: // OPEN_NEWWINDOW, OPEN_NEWTAB, OPEN_SWITCHTAB
                        return TARGET_WINDOW_NEW;
                }
            }

            /**
             * The URI to be loaded.
             */
            public final @NonNull String uri;

            /**
             * The URI of the origin page that triggered the load request.
             * null for initial loads and loads originating from data: URIs.
             */
            public final @Nullable String triggerUri;

            /**
             * The target where the window has requested to open.
             * One of {@link #TARGET_WINDOW_NONE TARGET_WINDOW_*}.
             */
            public final @TargetWindow int target;

            /**
             * True if and only if the request was triggered by an HTTP redirect.
             *
             * If the user loads URI "a", which redirects to URI "b", then
             * <code>onLoadRequest</code> will be called twice, first with uri "a" and
             * <code>isRedirect = false</code>, then with uri "b" and
             * <code>isRedirect = true</code>.
             */
            public final boolean isRedirect;
        }

        /**
         * A request to open an URI. This is called before each top-level page load to
         * allow custom behavior.
         * For example, this can be used to override the behavior of
         * TAGET_WINDOW_NEW requests, which defaults to requesting a new
         * GeckoSession via onNewSession.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param request The {@link LoadRequest} containing the request details.
         *
         * @return A {@link GeckoResult} with a AllowOrDeny value which indicates whether
         *         or not the load was handled. If unhandled, Gecko will continue the
         *         load as normal. If handled (true value), Gecko will abandon the load.
         *         A null return value is interpreted as false (unhandled).
         */
        @UiThread
        default @Nullable GeckoResult<AllowOrDeny> onLoadRequest(@NonNull GeckoSession session,
                                                                 @NonNull LoadRequest request) {
            return null;
        }

        /**
        * A request has been made to open a new session. The URI is provided only for
        * informational purposes. Do not call GeckoSession.loadUri() here. Additionally, the
        * returned GeckoSession must be a newly-created one.
        *
        * @param session The GeckoSession that initiated the callback.
        * @param uri The URI to be loaded.
        *
        * @return A {@link GeckoResult} which holds the returned GeckoSession. May be null, in
         *        which case the request for a new window by web content will fail. e.g.,
         *        <code>window.open()</code> will return null.
        */
        @UiThread
        default @Nullable GeckoResult<GeckoSession> onNewSession(@NonNull GeckoSession session,
                                                                 @NonNull String uri) {
            return null;
        }

        /**
         * @param session The GeckoSession that initiated the callback.
         * @param uri The URI that failed to load.
         * @param error A WebRequestError containing details about the error
         * @return A URI to display as an error. Returning null will halt the load entirely.
         */
        @UiThread
        default @Nullable GeckoResult<String> onLoadError(@NonNull GeckoSession session,
                                                          @Nullable String uri,
                                                          @NonNull WebRequestError error) {
            return null;
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({NavigationDelegate.TARGET_WINDOW_NONE, NavigationDelegate.TARGET_WINDOW_CURRENT,
            NavigationDelegate.TARGET_WINDOW_NEW})
    /* package */ @interface TargetWindow {}

    /**
     * GeckoSession applications implement this interface to handle prompts triggered by
     * content in the GeckoSession, such as alerts, authentication dialogs, and select list
     * pickers.
     **/
    public interface PromptDelegate {
        /**
         * Callback interface for notifying the result of a prompt, and for accessing the
         * optional features for prompts (e.g. optional checkbox).
         */
        interface AlertCallback {
            /**
             * Called by the prompt implementation when the prompt is dismissed without a
             * result, for example if the user presses the "Back" button. All prompts
             * must call dismiss() or confirm(), if available, when the prompt is dismissed.
             */
            @UiThread
            default void dismiss() {}

            /**
             * Return whether the prompt shown should include a checkbox. For example, if
             * a page shows multiple prompts within a short period of time, the next
             * prompt will include a checkbox to let the user disable future prompts.
             * Although the API allows checkboxes for all prompts, in practice, only
             * alert/button/text/auth prompts will possibly have a checkbox.
             *
             * @return True if prompt includes a checkbox.
             */
            @UiThread
            default boolean hasCheckbox() {
                return false;
            }

            /**
             * Return the message label for the optional checkbox.
             *
             * @return Checkbox message or null if none.
             */
            @UiThread
            @Nullable default String getCheckboxMessage() {
                return null;
            }

            /**
             * Return the initial value for the optional checkbox.
             *
             * @return Initial checkbox value.
             */
            @UiThread
            default boolean getCheckboxValue() {
                return false;
            }

            /**
             * Set the current value for the optional checkbox.
             *
             * @param value New checkbox value.
             */
            @UiThread
            default void setCheckboxValue(boolean value) {}
        }

        /**
         * Display a simple message prompt.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param callback Callback interface.
         */
        @UiThread
        default void onAlert(@NonNull GeckoSession session, @Nullable String title, @Nullable String msg,
                             @NonNull AlertCallback callback) {
            callback.dismiss();
        }

        /**
         * Callback interface for notifying the result of a button prompt.
         */
        interface ButtonCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the button prompt is dismissed by
             * the user pressing one of the buttons.
             *
             * @param button Button result; one of BUTTON_TYPE_* constants.
             */
            @UiThread
            default void confirm(int button) {}
        }

        static final int BUTTON_TYPE_POSITIVE = 0;
        static final int BUTTON_TYPE_NEUTRAL = 1;
        static final int BUTTON_TYPE_NEGATIVE = 2;

        /**
         * Display a prompt with up to three buttons.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param btnMsg Array of 3 elements indicating labels for the individual buttons.
         *               btnMsg[BUTTON_TYPE_POSITIVE] is the label for the "positive" button.
         *               btnMsg[BUTTON_TYPE_NEUTRAL] is the label for the "neutral" button.
         *               btnMsg[BUTTON_TYPE_NEGATIVE] is the label for the "negative" button.
         *               The button is hidden if the corresponding label is null.
         * @param callback Callback interface.
         */
        @UiThread
        default void onButtonPrompt(@NonNull GeckoSession session, @Nullable String title,
                                    @Nullable String msg, @Nullable String[] btnMsg,
                                    @NonNull ButtonCallback callback) {
            callback.dismiss();
        }

        /**
         * Callback interface for notifying the result of prompts that have text results,
         * including color and date/time pickers.
         */
        interface TextCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the text prompt is confirmed by
             * the user, for example by pressing the "OK" button.
             *
             * @param text Text result.
             */
            @UiThread
            default void confirm(@Nullable String text) {}
        }

        /**
         * Display a prompt for inputting text.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param value Default input text for the prompt.
         * @param callback Callback interface.
         */
        @UiThread
        default void onTextPrompt(@NonNull GeckoSession session, @Nullable String title,
                                  @Nullable String msg, @Nullable String value,
                                  @NonNull TextCallback callback) {
            callback.dismiss();
        }

        /**
         * Callback interface for notifying the result of authentication prompts.
         */
        interface AuthCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when a password-only prompt is
             * confirmed by the user.
             *
             * @param password Entered password.
             */
            @UiThread
            default void confirm(@Nullable String password) {}

            /**
             * Called by the prompt implementation when a username/password prompt is
             * confirmed by the user.
             *
             * @param username Entered username.
             * @param password Entered password.
             */
            @UiThread
            default void confirm(@NonNull String username, @NonNull String password) {}
        }

        class AuthOptions {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef(flag = true,
                    value = {AUTH_FLAG_HOST, AUTH_FLAG_PROXY,
                             AUTH_FLAG_ONLY_PASSWORD, AUTH_FLAG_PREVIOUS_FAILED,
                             AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE})
            /* package */ @interface AuthFlag {}

            /**
             * The auth prompt is for a network host.
             */
            public static final int AUTH_FLAG_HOST = 1;
            /**
             * The auth prompt is for a proxy.
             */
            public static final int AUTH_FLAG_PROXY = 2;
            /**
             * The auth prompt should only request a password.
             */
            public static final int AUTH_FLAG_ONLY_PASSWORD = 8;
            /**
             * The auth prompt is the result of a previous failed login.
             */
            public static final int AUTH_FLAG_PREVIOUS_FAILED = 16;
            /**
             * The auth prompt is for a cross-origin sub-resource.
             */
            public static final int AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE = 32;

            @Retention(RetentionPolicy.SOURCE)
            @IntDef({AUTH_LEVEL_NONE, AUTH_LEVEL_PW_ENCRYPTED, AUTH_LEVEL_SECURE})
            /* package */ @interface AuthLevel {}

            /**
             * The auth request is unencrypted or the encryption status is unknown.
             */
            public static final int AUTH_LEVEL_NONE = 0;
            /**
             * The auth request only encrypts password but not data.
             */
            public static final int AUTH_LEVEL_PW_ENCRYPTED = 1;
            /**
             * The auth request encrypts both password and data.
             */
            public static final int AUTH_LEVEL_SECURE = 2;

            /**
             * An int bit-field of AUTH_FLAG_* flags.
             */
            public @AuthFlag int flags;

            /**
             * A string containing the URI for the auth request or null if unknown.
             */
            public @Nullable String uri;

            /**
             * An int, one of AUTH_LEVEL_*, indicating level of encryption.
             */
            public @AuthLevel int level;

            /**
             * A string containing the initial username or null if password-only.
             */
            public @Nullable String username;

            /**
             * A string containing the initial password.
             */
            public @Nullable String password;

            /* package */ AuthOptions(final GeckoBundle options) {
                flags = options.getInt("flags");
                uri = options.getString("uri");
                level = options.getInt("level");
                username = options.getString("username");
                password = options.getString("password");
            }

            /**
             * Empty constructor for tests
             */
            protected AuthOptions() {
                flags = 0;
                uri = "";
                level = 0;
                username = "";
                password = "";
            }
        }

        /**
         * Display a prompt for authentication credentials.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param options AuthOptions containing options for the prompt
         * @param callback Callback interface.
         */
        @UiThread
        default void onAuthPrompt(final @NonNull GeckoSession session, final @Nullable String title,
                                  final @Nullable String msg, final @NonNull AuthOptions options,
                                  final @NonNull AuthCallback callback) {
            callback.dismiss();
        }

        class Choice {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({CHOICE_TYPE_MENU, CHOICE_TYPE_SINGLE, CHOICE_TYPE_MULTIPLE})
            /* package */ @interface ChoiceType {}

            /**
             * Display choices in a menu that dismisses as soon as an item is chosen.
             */
            public static final int CHOICE_TYPE_MENU = 1;

            /**
             * Display choices in a list that allows a single selection.
             */
            public static final int CHOICE_TYPE_SINGLE = 2;

            /**
             * Display choices in a list that allows multiple selections.
             */
            public static final int CHOICE_TYPE_MULTIPLE = 3;

            /**
             * A boolean indicating if the item is disabled. Item should not be
             * selectable if this is true.
             */
            public final boolean disabled;

            /**
             * A String giving the URI of the item icon, or null if none exists
             * (only valid for menus)
             */
            public final @Nullable String icon;

            /**
             * A String giving the ID of the item or group
             */
            public final @NonNull String id;

            /**
             * A Choice array of sub-items in a group, or null if not a group
             */
            public final @Nullable Choice[] items;

            /**
             * A string giving the label for displaying the item or group
             */
            public final @NonNull String label;

            /**
             * A boolean indicating if the item should be pre-selected
             * (pre-checked for menu items)
             */
            public final boolean selected;

            /**
             * A boolean indicating if the item should be a menu separator
             * (only valid for menus)
             */
            public final boolean separator;

            /* package */ Choice(final GeckoBundle choice) {
                disabled = choice.getBoolean("disabled");
                icon = choice.getString("icon");
                id = choice.getString("id");
                label = choice.getString("label");
                selected = choice.getBoolean("selected");
                separator = choice.getBoolean("separator");

                GeckoBundle[] choices = choice.getBundleArray("items");
                if (choices == null) {
                    items = null;
                } else {
                    items = new Choice[choices.length];
                    for (int i = 0; i < choices.length; i++) {
                        items[i] = new Choice(choices[i]);
                    }
                }
            }

            /**
             * Empty constructor for tests.
             */
            protected Choice() {
                disabled = false;
                icon = "";
                id = "";
                label = "";
                selected = false;
                separator = false;
                items = null;
            }
        }

        /**
         * Callback interface for notifying the result of menu or list choice.
         */
        interface ChoiceCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the menu or single-choice list is
             * dismissed by the user.
             *
             * @param id ID of the selected item.
             */
            @UiThread
            default void confirm(@Nullable String id) {}

            /**
             * Called by the prompt implementation when the multiple-choice list is
             * dismissed by the user.
             *
             * @param ids IDs of the selected items.
             */
            @UiThread
            default void confirm(@NonNull String[] ids) {}

            /**
             * Called by the prompt implementation when the menu or single-choice list is
             * dismissed by the user.
             *
             * @param item Choice representing the selected item; must be an original
             *             Choice object that was passed to the implementation.
             */
            @UiThread
            default void confirm(@NonNull Choice item) {}

            /**
             * Called by the prompt implementation when the multiple-choice list is
             * dismissed by the user.
             *
             * @param items Choice array representing the selected items; must be original
             *              Choice objects that were passed to the implementation.
             */
            @UiThread
            default void confirm(@Nullable Choice[] items) {}
        }


        /**
         * Display a menu prompt or list prompt.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog, or null for no title.
         * @param msg Message for the prompt dialog, or null for no message.
         * @param type One of CHOICE_TYPE_* indicating the type of prompt.
         * @param choices Array of Choices each representing an item or group.
         * @param callback Callback interface.
         */
        @UiThread
        default void onChoicePrompt(@NonNull GeckoSession session, @Nullable String title,
                                    @Nullable String msg, @Choice.ChoiceType int type,
                                    @NonNull Choice[] choices, @NonNull ChoiceCallback callback) {
            callback.dismiss();
        }

        /**
         * Display a color prompt.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param value Initial color value in HTML color format.
         * @param callback Callback interface; the result passed to confirm() must be in
         *                 HTML color format.
         */
        @UiThread
        default void onColorPrompt(@NonNull GeckoSession session, @Nullable String title,
                                   @Nullable String value, @NonNull TextCallback callback) {
            callback.dismiss();
        }

        /**
         * Prompt for year, month, and day.
         */
        static final int DATETIME_TYPE_DATE = 1;

        /**
         * Prompt for year and month.
         */
        static final int DATETIME_TYPE_MONTH = 2;

        /**
         * Prompt for year and week.
         */
        static final int DATETIME_TYPE_WEEK = 3;

        /**
         * Prompt for hour and minute.
         */
        static final int DATETIME_TYPE_TIME = 4;

        /**
         * Prompt for year, month, day, hour, and minute, without timezone.
         */
        static final int DATETIME_TYPE_DATETIME_LOCAL = 5;

        /**
         * Display a date/time prompt.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog; currently always null.
         * @param type One of DATETIME_TYPE_* indicating the type of prompt.
         * @param value Initial date/time value in HTML date/time format.
         * @param min Minimum date/time value in HTML date/time format.
         * @param max Maximum date/time value in HTML date/time format.
         * @param callback Callback interface; the result passed to confirm() must be in
         *                 HTML date/time format.
         */
        @UiThread
        default void onDateTimePrompt(@NonNull GeckoSession session, @Nullable String title,
                                      @DatetimeType int type, @Nullable String value, @Nullable String min,
                                      @Nullable String max, @NonNull TextCallback callback) {
            callback.dismiss();
        }

        /**
         * Callback interface for notifying the result of file prompts.
         */
        interface FileCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the user makes a file selection in
             * single-selection mode.
             *
             * @param context An application Context for parsing URIs.
             * @param uri The URI of the selected file.
             */
            @UiThread
            default void confirm(@Nullable Context context, @Nullable Uri uri) {}

            /**
             * Called by the prompt implementation when the user makes file selections in
             * multiple-selection mode.
             *
             * @param context An application Context for parsing URIs.
             * @param uris Array of URI objects for the selected files.
             */
            @UiThread
            default void confirm(@Nullable Context context, @Nullable Uri[] uris) {}
        }

        static final int FILE_TYPE_SINGLE = 1;
        static final int FILE_TYPE_MULTIPLE = 2;

        // These values should match the corresponding values in nsIFilePicker.idl
        /**
         * No capture attribute has been supplied by content.
         */
        static final int CAPTURE_TYPE_NONE = 0;

        /**
         * The capture attribute was supplied with a missing or invalid value.
         */
        static final int CAPTURE_TYPE_ANY = 1;

        /**
         * The "user" capture attribute has been supplied by content.
         */
        static final int CAPTURE_TYPE_USER = 2;

        /**
         * The "environment" capture attribute has been supplied by content.
         */
        static final int CAPTURE_TYPE_ENVIRONMENT = 3;

        /**
         * Display a file prompt.
         *
         * @param session GeckoSession that triggered the prompt
         * @param title Title for the prompt dialog.
         * @param type One of FILE_TYPE_* indicating the prompt type.
         * @param mimeTypes Array of permissible MIME types or extensions for the selected
         *                  files. MIME types are of the form "type/subtype", where "type"
         *                  and/or "subtype" can be "*" to indicate any value. Extensions
         *                  are of the form ".ext".
         * @param capture One of {@link #CAPTURE_TYPE_NONE CAPTURE_TYPE_*} indicating if the
         *                file is expected to be captured by a device camera and, if so, which
         *                type of camera.
         * @param callback Callback interface.
         */
        @UiThread
        default void onFilePrompt(@NonNull GeckoSession session, @Nullable String title,
                                  @FileType int type, @Nullable String[] mimeTypes,
                                  @CaptureType int capture, @NonNull FileCallback callback) {
            callback.dismiss();
        }

        /**
         * Display a popup request prompt; this occurs when content attempts to open
         * a new window in a way that doesn't appear to be the result of user input.
         *
         * @param session GeckoSession that triggered the prompt
         * @param targetUri The target URI for the popup
         *
         * @return A {@link GeckoResult} resolving to a AllowOrDeny which indicates
         *         whether or not the popup should be allowed to open.
         */
        @UiThread
        default @Nullable GeckoResult<AllowOrDeny> onPopupRequest(@NonNull GeckoSession session,
                                                                  @Nullable String targetUri) {
            return null;
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromptDelegate.FILE_TYPE_SINGLE, PromptDelegate.FILE_TYPE_MULTIPLE})
            /* package */ @interface FileType {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromptDelegate.CAPTURE_TYPE_NONE, PromptDelegate.CAPTURE_TYPE_ANY,
            PromptDelegate.CAPTURE_TYPE_USER, PromptDelegate.CAPTURE_TYPE_ENVIRONMENT})
    /* package */ @interface CaptureType {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromptDelegate.DATETIME_TYPE_DATE, PromptDelegate.DATETIME_TYPE_MONTH,
            PromptDelegate.DATETIME_TYPE_WEEK, PromptDelegate.DATETIME_TYPE_TIME,
            PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL})
    /* package */ @interface DatetimeType {}

    /**
     * GeckoSession applications implement this interface to handle content scroll
     * events.
     **/
    public interface ScrollDelegate {
        /**
         * The scroll position of the content has changed.
         *
        * @param session GeckoSession that initiated the callback.
        * @param scrollX The new horizontal scroll position in pixels.
        * @param scrollY The new vertical scroll position in pixels.
        */
        @UiThread
        default void onScrollChanged(@NonNull GeckoSession session, int scrollX, int scrollY) {}
    }

    /**
     * Get the PanZoomController instance for this session.
     *
     * @return PanZoomController instance.
     */
    @UiThread
    public @NonNull PanZoomController getPanZoomController() {
        ThreadUtils.assertOnUiThread();

        return mPanZoomController;
    }

    /**
     * Get the OverscrollEdgeEffect instance for this session.
     *
     * @return OverscrollEdgeEffect instance.
     */
    @UiThread
    public @NonNull OverscrollEdgeEffect getOverscrollEdgeEffect() {
        ThreadUtils.assertOnUiThread();

        if (mOverscroll == null) {
            mOverscroll = new OverscrollEdgeEffect(this);
        }
        return mOverscroll;
    }

    /**
     * Get the DynamicToolbarAnimator instance for this session.
     *
     * @return DynamicToolbarAnimator instance.
     */
    @UiThread
    public @NonNull DynamicToolbarAnimator getDynamicToolbarAnimator() {
        ThreadUtils.assertOnUiThread();

        if (mToolbar == null) {
            mToolbar = new DynamicToolbarAnimator(this);
        }
        return mToolbar;
    }

    /**
     * Get the CompositorController instance for this session.
     *
     * @return CompositorController instance.
     */
    @UiThread
    public @NonNull CompositorController getCompositorController() {
        ThreadUtils.assertOnUiThread();

        if (mController == null) {
            mController = new CompositorController(this);
            if (mCompositorReady) {
                mController.onCompositorReady();
            }
        }
        return mController;
    }

    /**
     * Get a matrix for transforming from client coordinates to surface coordinates.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getClientToScreenMatrix(Matrix)
     * @see #getPageToSurfaceMatrix(Matrix)
     */
    @UiThread
    public void getClientToSurfaceMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        matrix.setScale(mViewportZoom, mViewportZoom);
        if (mClientTop != mTop) {
            matrix.postTranslate(0, mClientTop - mTop);
        }
    }

    /**
     * Get a matrix for transforming from client coordinates to screen coordinates. The
     * client coordinates are in CSS pixels and are relative to the viewport origin; their
     * relation to screen coordinates does not depend on the current scroll position.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getClientToSurfaceMatrix(Matrix)
     * @see #getPageToScreenMatrix(Matrix)
     */
    @UiThread
    public void getClientToScreenMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getClientToSurfaceMatrix(matrix);
        matrix.postTranslate(mLeft, mTop);
    }

    /**
     * Get a matrix for transforming from page coordinates to screen coordinates. The page
     * coordinates are in CSS pixels and are relative to the page origin; their relation
     * to screen coordinates depends on the current scroll position of the outermost
     * frame.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getPageToSurfaceMatrix(Matrix)
     * @see #getClientToScreenMatrix(Matrix)
     */
    @UiThread
    public void getPageToScreenMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getPageToSurfaceMatrix(matrix);
        matrix.postTranslate(mLeft, mTop);
    }

    /**
     * Get a matrix for transforming from page coordinates to surface coordinates.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getPageToScreenMatrix(Matrix)
     * @see #getClientToSurfaceMatrix(Matrix)
     */
    @UiThread
    public void getPageToSurfaceMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getClientToSurfaceMatrix(matrix);
        matrix.postTranslate(-mViewportLeft, -mViewportTop);
    }

    /**
     * Get the bounds of the client area in client coordinates. The returned top-left
     * coordinates are always (0, 0). Use the matrix from {@link
     * #getClientToSurfaceMatrix(Matrix)} or {@link #getClientToScreenMatrix(Matrix)} to
     * map these bounds to surface or screen coordinates, respectively.
     *
     * @param rect RectF to be replaced by the client bounds in client coordinates.
     * @see #getSurfaceBounds(Rect)
     */
    @UiThread
    public void getClientBounds(@NonNull final RectF rect) {
        ThreadUtils.assertOnUiThread();

        rect.set(0.0f, 0.0f, (float) mWidth / mViewportZoom,
                (float) mClientHeight / mViewportZoom);
    }

    /**
     * Get the bounds of the client area in surface coordinates. This is equivalent to
     * mapping the bounds returned by #getClientBounds(RectF) with the matrix returned by
     * #getClientToSurfaceMatrix(Matrix).
     *
     * @param rect Rect to be replaced by the client bounds in surface coordinates.
     */
    @UiThread
    public void getSurfaceBounds(@NonNull final Rect rect) {
        ThreadUtils.assertOnUiThread();

        rect.set(0, mClientTop - mTop, mWidth, mHeight);
    }

    /**
     * GeckoSession applications implement this interface to handle requests for permissions
     * from content, such as geolocation and notifications. For each permission, usually
     * two requests are generated: one request for the Android app permission through
     * requestAppPermissions, which is typically handled by a system permission dialog;
     * and another request for the content permission (e.g. through
     * requestContentPermission), which is typically handled by an app-specific
     * permission dialog.
     *
     *
     * When denying an Android app permission, the response is not stored by GeckoView.
     * It is the responsibility of the consumer to store the response state and therefore prevent
     * further requests from being presented to the user.
     **/
    public interface PermissionDelegate {
        /**
         * Permission for using the geolocation API.
         * See: https://developer.mozilla.org/en-US/docs/Web/API/Geolocation
         */
        int PERMISSION_GEOLOCATION = 0;

        /**
         * Permission for using the notifications API.
         * See: https://developer.mozilla.org/en-US/docs/Web/API/notification
         */
        int PERMISSION_DESKTOP_NOTIFICATION = 1;

        /**
         * Permission for using the storage API.
         * See: https://developer.mozilla.org/en-US/docs/Web/API/Storage_API
         */
        int PERMISSION_PERSISTENT_STORAGE = 2;

        /**
         * Callback interface for notifying the result of a permission request.
         */
        interface Callback {
            /**
             * Called by the implementation after permissions are granted; the
             * implementation must call either grant() or reject() for every request.
             */
            @UiThread
            default void grant() {}

            /**
             * Called by the implementation when permissions are not granted; the
             * implementation must call either grant() or reject() for every request.
             */
            @UiThread
            default void reject() {}
        }

        /**
         * Request Android app permissions.
         *
         * @param session GeckoSession instance requesting the permissions.
         * @param permissions List of permissions to request; possible values are,
         *                    android.Manifest.permission.ACCESS_COARSE_LOCATION
         *                    android.Manifest.permission.ACCESS_FINE_LOCATION
         *                    android.Manifest.permission.CAMERA
         *                    android.Manifest.permission.RECORD_AUDIO
         * @param callback Callback interface.
         */
        @UiThread
        default void onAndroidPermissionsRequest(@NonNull GeckoSession session,
                                                 @Nullable String[] permissions,
                                                 @NonNull Callback callback) {
            callback.reject();
        }

        /**
         * Request content permission.
         *
         * Note, that in the case of PERMISSION_PERSISTENT_STORAGE, once permission has been granted
         * for a site, it cannot be revoked. If the permission has previously been granted, it is
         * the responsibility of the consuming app to remember the permission and prevent the prompt
         * from being redisplayed to the user.
         *
         * @param session GeckoSession instance requesting the permission.
         * @param uri The URI of the content requesting the permission.
         * @param type The type of the requested permission; possible values are,
         *             PERMISSION_GEOLOCATION
         *             PERMISSION_DESKTOP_NOTIFICATION
         *             PERMISSION_PERSISTENT_STORAGE
         * @param callback Callback interface.
         */
        @UiThread
        default void onContentPermissionRequest(@NonNull GeckoSession session, @Nullable String uri,
                                                @Permission int type, @NonNull Callback callback) {
            callback.reject();
        }

        class MediaSource {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({SOURCE_CAMERA, SOURCE_SCREEN,
                     SOURCE_MICROPHONE, SOURCE_AUDIOCAPTURE,
                     SOURCE_OTHER})
            /* package */ @interface Source {}

            /**
             * Constant to indicate that camera will be recorded.
             */
            public static final int SOURCE_CAMERA = 0;

            /**
             * Constant to indicate that screen will be recorded.
             */
            public static final int SOURCE_SCREEN  = 1;

            /**
             * Constant to indicate that microphone will be recorded.
             */
            public static final int SOURCE_MICROPHONE = 2;

            /**
             * Constant to indicate that device audio playback will be recorded.
             */
            public static final int SOURCE_AUDIOCAPTURE = 3;

            /**
             * Constant to indicate a media source that does not fall under the other categories.
             */
            public static final int SOURCE_OTHER = 4;

            @Retention(RetentionPolicy.SOURCE)
            @IntDef({TYPE_VIDEO, TYPE_AUDIO})
            /* package */ @interface Type {}

            /**
             * The media type is video.
             */
            public static final int TYPE_VIDEO = 0;

            /**
             * The media type is audio.
             */
            public static final int TYPE_AUDIO = 1;

            /**
             * A string giving the origin-specific source identifier.
             */
            public final @NonNull String id;

            /**
             * A string giving the non-origin-specific source identifier.
             */
            public final @NonNull String rawId;

            /**
             * A string giving the name of the video source from the system
             * (for example, "Camera 0, Facing back, Orientation 90").
             * May be empty.
             */
            public final @Nullable String name;

            /**
             * An int indicating the media source type.
             * Possible values for a video source are:
             * SOURCE_CAMERA, SOURCE_SCREEN, and SOURCE_OTHER.
             * Possible values for an audio source are:
             * SOURCE_MICROPHONE, SOURCE_AUDIOCAPTURE, and SOURCE_OTHER.
             */
            public final @Source int source;

            /**
             * An int giving the type of media, must be either TYPE_VIDEO or TYPE_AUDIO.
             */
            public final @Type int type;

            private static @Source int getSourceFromString(final String src) {
                // The strings here should match those in MediaSourceEnum in MediaStreamTrack.webidl
                if ("camera".equals(src)) {
                    return SOURCE_CAMERA;
                } else if ("screen".equals(src) || "window".equals(src) || "browser".equals(src)) {
                    return SOURCE_SCREEN;
                } else if ("microphone".equals(src)) {
                    return SOURCE_MICROPHONE;
                } else if ("audioCapture".equals(src)) {
                    return SOURCE_AUDIOCAPTURE;
                } else if ("other".equals(src) || "application".equals(src)) {
                    return SOURCE_OTHER;
                } else {
                    throw new IllegalArgumentException("String: " + src + " is not a valid media source string");
                }
            }

            private static @Type int getTypeFromString(final String type) {
                // The strings here should match the possible types in MediaDevice::MediaDevice in MediaManager.cpp
                if ("videoinput".equals(type)) {
                    return TYPE_VIDEO;
                } else if ("audioinput".equals(type)) {
                    return TYPE_AUDIO;
                } else {
                    throw new IllegalArgumentException("String: " + type + " is not a valid media type string");
                }
            }

            /* package */ MediaSource(final GeckoBundle media) {
                id = media.getString("id");
                rawId = media.getString("rawId");
                name = media.getString("name");
                source = getSourceFromString(media.getString("mediaSource"));
                type = getTypeFromString(media.getString("type"));
            }

            /**
             * Empty constructor for tests.
             */
            protected MediaSource() {
                id = null;
                rawId = null;
                name = null;
                source = 0;
                type = 0;
            }
        }

        /**
         * Callback interface for notifying the result of a media permission request,
         * including which media source(s) to use.
         */
        interface MediaCallback {
            /**
             * Called by the implementation after permissions are granted; the
             * implementation must call one of grant() or reject() for every request.
             *
             * @param video "id" value from the bundle for the video source to use,
             *              or null when video is not requested.
             * @param audio "id" value from the bundle for the audio source to use,
             *              or null when audio is not requested.
             */
            @UiThread
            default void grant(final @Nullable String video, final @Nullable String audio) {}

            /**
             * Called by the implementation after permissions are granted; the
             * implementation must call one of grant() or reject() for every request.
             *
             * @param video MediaSource for the video source to use (must be an original
             *              MediaSource object that was passed to the implementation);
             *              or null when video is not requested.
             * @param audio MediaSource for the audio source to use (must be an original
             *              MediaSource object that was passed to the implementation);
             *              or null when audio is not requested.
             */
            @UiThread
            default void grant(final @Nullable MediaSource video, final @Nullable MediaSource audio) {}

            /**
             * Called by the implementation when permissions are not granted; the
             * implementation must call one of grant() or reject() for every request.
             */
            @UiThread
            default void reject() {}
        }

        /**
         * Request content media permissions, including request for which video and/or
         * audio source to use.
         *
         * Media permissions will still be requested if the associated device permissions have been
         * denied if there are video or audio sources in that category that can still be accessed.
         * It is the responsibility of consumers to ensure that media permission requests are not
         * displayed in this case.
         *
         * @param session GeckoSession instance requesting the permission.
         * @param uri The URI of the content requesting the permission.
         * @param video List of video sources, or null if not requesting video.
         * @param audio List of audio sources, or null if not requesting audio.
         * @param callback Callback interface.
         */
        @UiThread
        default void onMediaPermissionRequest(@NonNull GeckoSession session, @NonNull String uri,
                                              @Nullable MediaSource[] video, @Nullable MediaSource[] audio,
                                              @NonNull MediaCallback callback) {
            callback.reject();
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PermissionDelegate.PERMISSION_GEOLOCATION,
            PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION})
    /* package */ @interface Permission {}

    /**
     * Interface that SessionTextInput uses for performing operations such as opening and closing
     * the software keyboard. If the delegate is not set, these operations are forwarded to the
     * system {@link android.view.inputmethod.InputMethodManager} automatically.
     */
    public interface TextInputDelegate {
        /** Restarting input due to an input field gaining focus. */
        int RESTART_REASON_FOCUS = 0;
        /** Restarting input due to an input field losing focus. */
        int RESTART_REASON_BLUR = 1;
        /**
         * Restarting input due to the content of the input field changing. For example, the
         * input field type may have changed, or the current composition may have been committed
         * outside of the input method.
         */
        int RESTART_REASON_CONTENT_CHANGE = 2;

        /**
         * Reset the input method, and discard any existing states such as the current composition
         * or current autocompletion. Because the current focused editor may have changed, as
         * part of the reset, a custom input method would normally call {@link
         * SessionTextInput#onCreateInputConnection} to update its knowledge of the focused editor.
         * Note that {@code restartInput} should be used to detect changes in focus, rather than
         * {@link #showSoftInput} or {@link #hideSoftInput}, because focus changes are not always
         * accompanied by requests to show or hide the soft input. This method is always called,
         * even in viewless mode.
         *
         * @param session Session instance.
         * @param reason Reason for the reset.
         */
        @UiThread
        default void restartInput(@NonNull GeckoSession session, @RestartReason int reason) {}

        /**
         * Display the soft input. May be called consecutively, even if the soft input is
         * already shown. This method is always called, even in viewless mode.
         *
         * @param session Session instance.
         * @see #hideSoftInput
         * */
        @UiThread
        default void showSoftInput(@NonNull GeckoSession session) {}

        /**
         * Hide the soft input. May be called consecutively, even if the soft input is
         * already hidden. This method is always called, even in viewless mode.
         *
         * @param session Session instance.
         * @see #showSoftInput
         * */
        @UiThread
        default void hideSoftInput(@NonNull GeckoSession session) {}

        /**
         * Update the soft input on the current selection. This method is <i>not</i> called
         * in viewless mode.
         *
         * @param session Session instance.
         * @param selStart Start offset of the selection.
         * @param selEnd End offset of the selection.
         * @param compositionStart Composition start offset, or -1 if there is no composition.
         * @param compositionEnd Composition end offset, or -1 if there is no composition.
         */
        @UiThread
        default void updateSelection(@NonNull GeckoSession session, int selStart, int selEnd,
                                     int compositionStart, int compositionEnd) {}

        /**
         * Update the soft input on the current extracted text, as requested through
         * {@link android.view.inputmethod.InputConnection#getExtractedText}.
         * Consequently, this method is <i>not</i> called in viewless mode.
         *
         * @param session Session instance.
         * @param request The extract text request.
         * @param text The extracted text.
         */
        @UiThread
        default void updateExtractedText(@NonNull GeckoSession session,
                                         @NonNull ExtractedTextRequest request,
                                         @NonNull ExtractedText text) {}

        /**
         * Update the cursor-anchor information as requested through
         * {@link android.view.inputmethod.InputConnection#requestCursorUpdates}.
         * Consequently, this method is <i>not</i> called in viewless mode.
         *
         * @param session Session instance.
         * @param info Cursor-anchor information.
         */
        @UiThread
        default void updateCursorAnchorInfo(@NonNull GeckoSession session,
                                            @NonNull CursorAnchorInfo info) {}

        /** An auto-fill session has started, usually as a result of loading a page. */
        int AUTO_FILL_NOTIFY_STARTED = 0;
        /** An auto-fill session has been committed, usually as a result of submitting a form. */
        int AUTO_FILL_NOTIFY_COMMITTED = 1;
        /** An auto-fill session has been canceled, usually as a result of unloading a page. */
        int AUTO_FILL_NOTIFY_CANCELED = 2;
        /** A view within the auto-fill session has been added. */
        int AUTO_FILL_NOTIFY_VIEW_ADDED = 3;
        /** A view within the auto-fill session has been removed. */
        int AUTO_FILL_NOTIFY_VIEW_REMOVED = 4;
        /** A view within the auto-fill session has been updated (e.g. change in state). */
        int AUTO_FILL_NOTIFY_VIEW_UPDATED = 5;
        /** A view within the auto-fill session has gained focus. */
        int AUTO_FILL_NOTIFY_VIEW_ENTERED = 6;
        /** A view within the auto-fill session has lost focus. */
        int AUTO_FILL_NOTIFY_VIEW_EXITED = 7;

        /**
         * Notify that an auto-fill event has occurred. The default implementation forwards the
         * notification to the system {@link android.view.autofill.AutofillManager}. This method is
         * only called on Android 6.0 and above, and it is called in viewless mode as well.
         *
         * @param session Session instance.
         * @param notification Notification type as one of the {@link #AUTO_FILL_NOTIFY_STARTED
         *                     AUTO_FILL_NOTIFY_*} constants.
         * @param virtualId Virtual ID of the target, or {@link android.view.View#NO_ID} if not
         *                  applicable. The ID matches one of the virtual IDs provided by {@link
         *                  SessionTextInput#onProvideAutofillVirtualStructure} and can be used
         *                  with {@link SessionTextInput#autofill}.
         */
        @UiThread
        default void notifyAutoFill(@NonNull GeckoSession session,
                                    @AutoFillNotification int notification,
                                    int virtualId) {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({TextInputDelegate.RESTART_REASON_FOCUS, TextInputDelegate.RESTART_REASON_BLUR,
            TextInputDelegate.RESTART_REASON_CONTENT_CHANGE})
            /* package */ @interface RestartReason {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            TextInputDelegate.AUTO_FILL_NOTIFY_STARTED,
            TextInputDelegate.AUTO_FILL_NOTIFY_COMMITTED,
            TextInputDelegate.AUTO_FILL_NOTIFY_CANCELED,
            TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ADDED,
            TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_REMOVED,
            TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_UPDATED,
            TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED,
            TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_EXITED})
    /* package */ @interface AutoFillNotification {}

    /* package */ void onSurfaceChanged(final Surface surface, final int x, final int y, final int width,
                                        final int height) {
        ThreadUtils.assertOnUiThread();

        mOffsetX = x;
        mOffsetY = y;
        mWidth = width;
        mHeight = height;

        if (mCompositorReady) {
            mCompositor.syncResumeResizeCompositor(x, y, width, height, surface);
            onWindowBoundsChanged();
            return;
        }

        // We have a valid surface but we're not attached or the compositor
        // is not ready; save the surface for later when we're ready.
        mSurface = surface;

        // Adjust bounds as the last step.
        onWindowBoundsChanged();
    }

    /* package */ void onSurfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        if (mCompositorReady) {
            mCompositor.syncPauseCompositor();
            return;
        }

        // While the surface was valid, we never became attached or the
        // compositor never became ready; clear the saved surface.
        mSurface = null;
    }

    /* package */ void onScreenOriginChanged(final int left, final int top) {
        ThreadUtils.assertOnUiThread();

        if (mLeft == left && mTop == top) {
            return;
        }

        mLeft = left;
        mTop = top;
        onWindowBoundsChanged();
    }

    /* package */ void setFixedBottomOffset(final int offset) {
        mFixedBottomOffset = offset;

        if (mCompositorReady) {
            mCompositor.setFixedBottomOffset(mFixedBottomOffset);
        }
    }


    /* package */ void onCompositorAttached() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mAttachedCompositor = true;
        mCompositor.attachNPZC(mPanZoomController.mNative);

        if (mSurface != null) {
            // If we have a valid surface, create the compositor now that we're attached.
            // Leave mSurface alone because we'll need it later for onCompositorReady.
            onSurfaceChanged(mSurface, mOffsetX, mOffsetY, mWidth, mHeight);
        }

        mCompositor.sendToolbarAnimatorMessage(IS_COMPOSITOR_CONTROLLER_OPEN);
    }

    /* package */ void onCompositorDetached() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mController != null) {
            mController.onCompositorDetached();
        }

        mAttachedCompositor = false;
        mCompositorReady = false;
    }

    /* package */ void handleCompositorMessage(final int message) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        switch (message) {
            case COMPOSITOR_CONTROLLER_OPEN: {
                if (isCompositorReady()) {
                    return;
                }

                // Delay calling onCompositorReady to avoid deadlock due
                // to synchronous call to the compositor.
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        onCompositorReady();
                    }
                });
                break;
            }

            case FIRST_PAINT: {
                if (mController != null) {
                    mController.onFirstPaint();
                }
                ContentDelegate delegate = mContentHandler.getDelegate();
                if (delegate != null) {
                    delegate.onFirstComposite(this);
                }
                break;
            }

            case LAYERS_UPDATED: {
                if (mController != null) {
                    mController.notifyDrawCallbacks();
                }
                break;
            }

            case STATIC_TOOLBAR_READY:
            case TOOLBAR_SHOW: {
                if (mToolbar != null) {
                    mToolbar.handleToolbarAnimatorMessage(message);
                    // Update window bounds due to toolbar visibility change.
                    onWindowBoundsChanged();
                }
                break;
            }

            default: {
                if (mToolbar != null) {
                    mToolbar.handleToolbarAnimatorMessage(message);
                } else {
                    Log.w(LOGTAG, "Unexpected message: " + message);
                }
                break;
            }
        }
    }

    /* package */ boolean isCompositorReady() {
        return mCompositorReady;
    }

    /* package */ void onCompositorReady() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (!mAttachedCompositor) {
            return;
        }

        mCompositorReady = true;

        if (mController != null) {
            mController.onCompositorReady();
        }

        if (mSurface != null) {
            // If we have a valid surface, resume the
            // compositor now that the compositor is ready.
            onSurfaceChanged(mSurface, mOffsetX, mOffsetY, mWidth, mHeight);
            mSurface = null;
        }

        if (mToolbar != null) {
            mToolbar.onCompositorReady();
        }

        mCompositor.setFixedBottomOffset(mFixedBottomOffset);
    }

    /* package */ void updateOverscrollVelocity(final float x, final float y) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mOverscroll == null) {
            return;
        }

        // Multiply the velocity by 1000 to match what was done in JPZ.
        mOverscroll.setVelocity(x * 1000.0f, OverscrollEdgeEffect.AXIS_X);
        mOverscroll.setVelocity(y * 1000.0f, OverscrollEdgeEffect.AXIS_Y);
    }

    /* package */ void updateOverscrollOffset(final float x, final float y) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mOverscroll == null) {
            return;
        }

        mOverscroll.setDistance(x, OverscrollEdgeEffect.AXIS_X);
        mOverscroll.setDistance(y, OverscrollEdgeEffect.AXIS_Y);
    }

    /* package */ void onMetricsChanged(final float scrollX, final float scrollY,
                                        final float zoom) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mViewportLeft = scrollX;
        mViewportTop = scrollY;
        mViewportZoom = zoom;

        mTextInput.onScreenMetricsUpdated();
    }

    /* protected */ void onWindowBoundsChanged() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        final int toolbarHeight;
        if (mToolbar != null) {
            toolbarHeight = mToolbar.getCurrentToolbarHeight();
        } else {
            toolbarHeight = 0;
        }

        mClientTop = mTop + toolbarHeight;
        mClientHeight = mHeight - toolbarHeight;

        if (mAttachedCompositor) {
            mCompositor.onBoundsChanged(mLeft, mClientTop, mWidth, mClientHeight);
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(mWidth, mClientHeight);
        }
    }

    /**
     * GeckoSession applications implement this interface to handle media events.
     */
    public interface MediaDelegate {

        class RecordingDevice {

            /*
             * Default status flags for this RecordingDevice.
             */
            public static class Status {
                public static final long RECORDING = 0;
                public static final long INACTIVE = 1 << 0;

                // Do not instantiate this class.
                protected Status() {}
            }

            /*
             * Default device types for this RecordingDevice.
             */
            public static class Type {
                public static final long CAMERA = 0;
                public static final long MICROPHONE = 1 << 0;

                // Do not instantiate this class.
                protected Type() {}
            }

            @Retention(RetentionPolicy.SOURCE)
            @LongDef(flag = true,
                    value = { Status.RECORDING, Status.INACTIVE })
            /* package */ @interface RecordingStatus {}

            @Retention(RetentionPolicy.SOURCE)
            @LongDef(flag = true,
                    value = {Type.CAMERA, Type.MICROPHONE})
            /* package */ @interface DeviceType {}

            /**
             * A long giving the current recording status, must be either Status.RECORDING,
             * Status.PAUSED or Status.INACTIVE.
             */
            public final @RecordingStatus long status;

            /**
             * A long giving the type of the recording device, must be either Type.CAMERA or Type.MICROPHONE.
             */
            public final @DeviceType long type;

            private static @DeviceType long getTypeFromString(final String type) {
                if ("microphone".equals(type)) {
                    return Type.MICROPHONE;
                } else if ("camera".equals(type)) {
                    return Type.CAMERA;
                } else {
                    throw new IllegalArgumentException("String: " + type + " is not a valid recording device string");
                }
            }

            private static @RecordingStatus long getStatusFromString(final String type) {
                if ("recording".equals(type)) {
                    return Status.RECORDING;
                } else {
                    return Status.INACTIVE;
                }
            }

            /* package */ RecordingDevice(final GeckoBundle media) {
                status = getStatusFromString(media.getString("status"));
                type = getTypeFromString(media.getString("type"));
            }

            /**
             * Empty constructor for tests.
             */
            protected RecordingDevice() {
                status = Status.INACTIVE;
                type = Type.CAMERA;
            }
        }
        /**
         * An HTMLMediaElement has been created.
         * @param session Session instance.
         * @param element The media element that was just created.
         */
        @UiThread
        default void onMediaAdd(@NonNull GeckoSession session, @NonNull MediaElement element) {}

        /**
         * An HTMLMediaElement has been unloaded.
         * @param session Session instance.
         * @param element The media element that was unloaded.
         */
        @UiThread
        default void onMediaRemove(@NonNull GeckoSession session, @NonNull MediaElement element) {}

        /**
         * A recording device has changed state.
         * Any change to the recording state of the devices microphone or camera will call this
         * delegate method. The argument provides details of the active recording devices.
         * @param session The session that the event has originated from.
         * @param devices The list of active devices and their recording state.
         */
        @UiThread
        default void onRecordingStatusChanged(@NonNull GeckoSession session, @NonNull RecordingDevice[] devices) {}
    }

    /**
     * An interface for recording new history visits and fetching the visited
     * status for links.
     */
    public interface HistoryDelegate {
        /**
         * A representation of an entry in browser history.
         */
        public interface HistoryItem {
            /**
             * Get the URI of this history element.
             *
             * @return A String representing the URI of this history element.
             */
            @AnyThread
            default @NonNull String getUri() {
                throw new UnsupportedOperationException("HistoryItem.getUri() called on invalid object.");
            }

            /**
             * Get the title of this history element.
             *
             * @return A String representing the title of this history element.
             */
            @AnyThread
            default @NonNull String getTitle() {
                throw new UnsupportedOperationException("HistoryItem.getString() called on invalid object.");
            }
        }

        /**
         * A representation of browser history, accessible as a `List`. The list itself
         * and its entries are immutable; any attempt to mutate will result in an
         * `UnsupportedOperationException`.
         */
        public interface HistoryList extends List<HistoryItem> {
            /**
             * Get the current index in browser history.
             *
             * @return An int representing the current index in browser history.
             */
            @AnyThread
            default int getCurrentIndex() {
                throw new UnsupportedOperationException("HistoryList.getCurrentIndex() called on invalid object.");
            }
        }

        // These flags are similar to those in `IHistory::LoadFlags`, but we use
        // different values to decouple GeckoView from Gecko changes. These
        // should be kept in sync with `GeckoViewHistory::GeckoViewVisitFlags`.

        /** The URL was visited a top-level window. */
        final int VISIT_TOP_LEVEL = 1 << 0;
        /** The URL is the target of a temporary redirect. */
        final int VISIT_REDIRECT_TEMPORARY = 1 << 1;
        /** The URL is the target of a permanent redirect. */
        final int VISIT_REDIRECT_PERMANENT = 1 << 2;
        /** The URL is temporarily redirected to another URL. */
        final int VISIT_REDIRECT_SOURCE = 1 << 3;
        /** The URL is permanently redirected to another URL. */
        final int VISIT_REDIRECT_SOURCE_PERMANENT = 1 << 4;
        /** The URL failed to load due to a client or server error. */
        final int VISIT_UNRECOVERABLE_ERROR = 1 << 5;

        /**
         * Records a visit to a page.
         *
         * @param session The session where the URL was visited.
         * @param url The visited URL.
         * @param lastVisitedURL The last visited URL in this session, to detect
         *                       redirects and reloads.
         * @param flags Additional flags for this visit, including redirect and
         *              error statuses. This is a bitmask of one or more
         *              {@link #VISIT_TOP_LEVEL VISIT_*} flags, OR-ed together.
         * @return A {@link GeckoResult} completed with a boolean indicating
         *         whether to highlight links for the new URL as visited
         *         ({@code true}) or unvisited ({@code false}).
         */
        @UiThread
        default @Nullable GeckoResult<Boolean> onVisited(@NonNull GeckoSession session,
                                                         @NonNull String url,
                                                         @Nullable String lastVisitedURL,
                                                         @VisitFlags int flags) {
            return null;
        }

        /**
         * Returns the visited statuses for links on a page. This is used to
         * highlight links as visited or unvisited, for example.
         *
         * @param session The session requesting the visited statuses.
         * @param urls A list of URLs to check.
         * @return A {@link GeckoResult} completed with a list of booleans
         *         corresponding to the URLs in {@code urls}, and indicating
         *         whether to highlight links for each URL as visited
         *         ({@code true}) or unvisited ({@code false}).
         */
        @UiThread
        default @Nullable GeckoResult<boolean[]> getVisited(@NonNull GeckoSession session,
                                                            @NonNull String[] urls) {
            return null;
        }

        @UiThread
        default void onHistoryStateChange(@NonNull GeckoSession session, @NonNull HistoryList historyList) {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = {
                HistoryDelegate.VISIT_TOP_LEVEL,
                HistoryDelegate.VISIT_REDIRECT_TEMPORARY,
                HistoryDelegate.VISIT_REDIRECT_PERMANENT,
                HistoryDelegate.VISIT_REDIRECT_SOURCE,
                HistoryDelegate.VISIT_REDIRECT_SOURCE_PERMANENT,
                HistoryDelegate.VISIT_UNRECOVERABLE_ERROR
            })
    /* package */ @interface VisitFlags {}
}
