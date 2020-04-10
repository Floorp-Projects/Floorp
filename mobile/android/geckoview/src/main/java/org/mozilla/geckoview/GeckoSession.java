/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.io.ByteArrayInputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.AbstractSequentialList;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.UUID;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
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
import android.database.Cursor;
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
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;
import android.util.LongSparseArray;
import android.util.SparseArray;
import android.view.Surface;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.View;
import android.view.ViewStructure;

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
    private Autofill.Support mAutofillSupport;

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
    private int mFixedBottomOffset = 0; // The margin for fixed elements attached to the bottom of the viewport.
    private int mDynamicToolbarMaxHeight = 0; // The maximum height of the dynamic toolbar
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

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        public native void setDynamicToolbarMaxHeight(int height);

        // Gecko thread pauses compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncPauseCompositor();

        // UI thread resumes compositor and notifies Gecko thread; does not block UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncResumeResizeCompositor(int x, int y, int width, int height, Object surface);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setMaxToolbarHeight(int height);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
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
        /* package */ native void requestScreenPixels(final GeckoResult<ByteBuffer> result,
                                                      final int x, final int y,
                                                      final int srcWidth, final int srcHeight,
                                                      final int outWidth, final int outHeight);

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

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        public native void onSafeAreaInsetsChanged(int top, int right, int bottom, int left);

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
                "GeckoView:StateUpdated",
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
                } else if ("GeckoView:StateUpdated".equals(event)) {

                    final GeckoBundle update = message.getBundle("data");

                    if (update == null) {
                        return;
                    }
                    final int previousHistorySize = mStateCache.size();
                    mStateCache.updateSessionState(update);

                    ProgressDelegate progressDelegate = getProgressDelegate();
                    if (progressDelegate != null) {
                        progressDelegate.onSessionStateChange(GeckoSession.this, new SessionState(mStateCache));
                    }

                    if (update.getBundle("historychange") != null) {
                        final SessionState state = new SessionState(mStateCache);

                        delegate.onHistoryStateChange(GeckoSession.this, state);

                        // If the previous history was larger than one entry and the new size is one, it means the
                        // History has been purged and the navigation delegate needs to be update.
                        if ((previousHistorySize > 1) && (state.size() == 1) && mNavigationHandler.getDelegate() != null) {
                            mNavigationHandler.getDelegate().onCanGoForward(GeckoSession.this, false);
                            mNavigationHandler.getDelegate().onCanGoBack(GeckoSession.this, false);
                        }
                    }
                }
            }
        };

    private final WebExtension.SessionController mWebExtensionController;

    private final GeckoSessionHandler<ContentDelegate> mContentHandler =
        new GeckoSessionHandler<ContentDelegate>(
            "GeckoViewContent", this,
            new String[]{
                "GeckoView:ContentCrash",
                "GeckoView:ContentKill",
                "GeckoView:ContextMenu",
                "GeckoView:DOMMetaViewportFit",
                "GeckoView:DOMTitleChanged",
                "GeckoView:DOMWindowClose",
                "GeckoView:ExternalResponse",
                "GeckoView:FocusRequest",
                "GeckoView:FullScreenEnter",
                "GeckoView:FullScreenExit",
                "GeckoView:WebAppManifest",
                "GeckoView:FirstContentfulPaint",
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

                } else if ("GeckoView:DOMMetaViewportFit".equals(event)) {
                    delegate.onMetaViewportFitChange(GeckoSession.this,
                                                     message.getString("viewportfit"));
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
                        delegate.onWebAppManifest(GeckoSession.this, fixupWebAppManifest(manifest.toJSONObject()));
                    } catch (JSONException e) {
                        Log.e(LOGTAG, "Failed to convert web app manifest to JSON", e);
                    }
                } else if ("GeckoView:FirstContentfulPaint".equals(event)) {
                    delegate.onFirstContentfulPaint(GeckoSession.this);
                }
            }
        };

    private final GeckoSessionHandler<NavigationDelegate> mNavigationHandler =
        new GeckoSessionHandler<NavigationDelegate>(
            "GeckoViewNavigation", this,
            new String[]{
                "GeckoView:LocationChange",
                "GeckoView:OnNewSession"
            },
            new String[] {
                "GeckoView:OnLoadError",
                "GeckoView:OnLoadRequest",
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
            public void handleDefaultMessage(final String event,
                                             final GeckoBundle message,
                                             final EventCallback callback) {

                if ("GeckoView:OnLoadRequest".equals(event)) {
                    callback.sendSuccess(false);
                } else if ("GeckoView:OnLoadError".equals(event)) {
                    callback.sendSuccess(null);
                } else {
                    super.handleDefaultMessage(event, message, callback);
                }
            }

            @Override
            public void handleMessage(final NavigationDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                Log.d(LOGTAG, "handleMessage " + event + " uri=" + message.getString("uri"));
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
                              message.getInt("flags"),
                              message.getBoolean("hasUserGesture"),
                              /* isDirectNavigation */ false);

                    if (!IntentUtils.isUriSafeForScheme(request.uri)) {
                        callback.sendError("Blocked unsafe intent URI");

                        delegate.onLoadError(GeckoSession.this, request.uri,
                                             new WebRequestError(WebRequestError.ERROR_MALFORMED_URI,
                                                                 WebRequestError.ERROR_CATEGORY_URI,
                                                                 null));

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

                    final WebRequestError err = WebRequestError.fromGeckoError(errorCode, errorModule, errorClass, null);

                    final GeckoResult<String> result = delegate.onLoadError(GeckoSession.this, uri, err);
                    if (result == null) {
                        callback.sendError("abort");
                        return;
                    }

                    result.accept(url -> {
                        if (url == null) {
                            callback.sendError("abort");
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

    private final GeckoSessionHandler<ContentDelegate> mProcessHangHandler =
        new GeckoSessionHandler<ContentDelegate>(
                "GeckoViewProcessHangMonitor", this,
                new String[]{"GeckoView:HangReport"}) {


            @Override
            protected void handleMessage(final ContentDelegate delegate,
                                         final String event,
                                         final GeckoBundle message,
                                         final EventCallback eventCallback) {
                Log.d(LOGTAG, "handleMessage " + event + " uri=" + message.getString("uri"));

                GeckoResult<SlowScriptResponse> result = delegate.onSlowScript(GeckoSession.this,
                        message.getString("scriptFileName"));
                if (result != null) {
                    final int mReportId = message.getInt("hangId");
                    result.accept(stopOrContinue -> {
                        if (stopOrContinue != null) {
                            final GeckoBundle bundle = new GeckoBundle();
                            bundle.putInt("hangId", mReportId);
                            switch (stopOrContinue) {
                                case STOP:
                                    mEventDispatcher.dispatch("GeckoView:HangReportStop", bundle);
                                    break;
                                case CONTINUE:
                                    mEventDispatcher.dispatch("GeckoView:HangReportWait", bundle);
                                    break;
                            }
                        }
                    });
                } else {
                    // default to stopping the script
                    final GeckoBundle bundle = new GeckoBundle();
                    bundle.putInt("hangId", message.getInt("hangId"));
                    mEventDispatcher.dispatch("GeckoView:HangReportStop", bundle);
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
                "GeckoView:StateUpdated",
            }
        ) {
            @Override
            public void handleMessage(final ProgressDelegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                Log.d(LOGTAG, "handleMessage " + event + " uri=" + message.getString("uri"));
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
                    final GeckoBundle update = message.getBundle("data");
                    if (update != null) {
                        if (getHistoryDelegate() == null) {
                            mStateCache.updateSessionState(update);
                            delegate.onSessionStateChange(GeckoSession.this, new SessionState(mStateCache));
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
            new String[]{ "GeckoView:ContentBlockingEvent" }
        ) {
            @Override
            public void handleMessage(final ContentBlocking.Delegate delegate,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if ("GeckoView:ContentBlockingEvent".equals(event)) {
                    final ContentBlocking.BlockEvent be =
                        ContentBlocking.BlockEvent.fromBundle(message);
                    if (be.isBlocking()) {
                        delegate.onContentBlocked(GeckoSession.this, be);
                    } else {
                        delegate.onContentLoaded(GeckoSession.this, be);
                    }
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
                    } else if ("xr".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_XR;
                    } else if ("midi".equals(typeString)) {
                        // We can get this from WPT and presumably other content, but Gecko
                        // doesn't support Web MIDI.
                        callback.sendError("Unsupported");
                        return;
                    } else if ("autoplay-media-inaudible".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_AUTOPLAY_INAUDIBLE;
                    } else if ("autoplay-media-audible".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_AUTOPLAY_AUDIBLE;
                    } else if ("media-key-system-access".equals(typeString)) {
                        type = PermissionDelegate.PERMISSION_MEDIA_KEY_SYSTEM_ACCESS;
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
                    final @SelectionActionDelegateAction HashSet<String> actionsSet =
                            new HashSet<>(Arrays.asList(message.getStringArray("actions")));
                    final SelectionActionDelegate.Selection selection =
                            new SelectionActionDelegate.Selection(message, actionsSet, callback);

                    delegate.onShowActionRequest(GeckoSession.this, selection);

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
        mPermissionHandler, mProcessHangHandler, mProgressHandler, mScrollHandler,
        mSelectionActionDelegate, mContentBlockingHandler
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
     * Get the current permission delegate for this GeckoSession.
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
                                       int screenId, boolean privateMode, boolean isRemote);

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

        @WrapForJNI(calledFrom = "gecko")
        private GeckoResult<Boolean> onLoadRequest(final @NonNull String uri, final int windowType,
                                                   final int flags, final @Nullable String triggeringUri,
                                                   final boolean hasUserGesture) {
            final GeckoSession session = (mOwner == null) ? null : mOwner.get();
            if (session == null) {
                // Don't handle any load request if we can't get the session for some reason.
                return GeckoResult.fromValue(false);
            }
            GeckoResult<Boolean> res = new GeckoResult<>();

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    final NavigationDelegate delegate = session.getNavigationDelegate();

                    if (delegate == null) {
                        res.complete(false);
                        return;
                    }

                    final String trigger = TextUtils.isEmpty(triggeringUri) ? null : triggeringUri;
                    final NavigationDelegate.LoadRequest req = new NavigationDelegate.LoadRequest(uri,
                            trigger, windowType, flags, hasUserGesture, false /* isDirectNavigation */);
                    final GeckoResult<AllowOrDeny> reqResponse = delegate.onLoadRequest(session, req);

                    if (reqResponse == null) {
                        res.complete(false);
                        return;
                    }

                    reqResponse.accept(value -> {
                        if (value == AllowOrDeny.DENY) {
                            res.complete(true);
                        } else {
                            res.complete(false);
                        }
                    }, ex -> {
                        // This is incredibly ugly and unreadable because checkstyle sucks.
                            res.complete(false);
                        });
                }
            });

            return res;
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

        mWebExtensionController = new WebExtension.SessionController(this);

        mAutofillSupport = new Autofill.Support(this);
        mAutofillSupport.registerListeners();

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
            mWebExtensionController.setRuntime(mWindow.runtime);
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
        final boolean isRemote = runtime.getSettings().getUseMultiprocess();

        mWindow = new Window(runtime, this, mNativeQueue);
        mWebExtensionController.setRuntime(runtime);

        onWindowChanged(WINDOW_OPEN, /* inProgress */ true);

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            Window.open(mWindow, mNativeQueue, mCompositor, mEventDispatcher,
                        mAccessibility != null ? mAccessibility.nativeProvider : null,
                        createInitData(), mId, chromeUri, screenId, isPrivate, isRemote);
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
                screenId, isPrivate, isRemote);
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
            getAutofillSupport().clear();
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
     * Formats the map of additional request headers into an ArrayList
     * @param additionalHeaders Request headers to be formatted
     * @return Correctly formatted request headers as a ArrayList
     */
    private ArrayList<String> additionalHeadersToStringArray(final @NonNull Map<String, String> additionalHeaders) {
        ArrayList<String> headers = new ArrayList<String>();
        for (String key : additionalHeaders.keySet()) {
            // skip null key if one exists
            if (key == null)
                continue;

            headers.add( String.format("%s:%s", key, additionalHeaders.get(key)) );
        }
        return headers;
    }

    /**
     * Load the given URI.
     * @param uri The URI of the resource to load.
     */
    @AnyThread
    public void loadUri(final @NonNull String uri) {
        loadUri(uri, (GeckoSession)null, LOAD_FLAGS_NONE, (Map<String, String>) null);
    }

    /**
     * Load the given URI with specified HTTP request headers.
     * @param uri The URI of the resource to load.
     * @param additionalHeaders any additional request headers used with the load
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @Nullable Map<String, String> additionalHeaders) {
        loadUri(uri, (GeckoSession)null, LOAD_FLAGS_NONE, additionalHeaders);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     *
     * @param uri the URI to load
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @LoadFlags int flags) {
        loadUri(uri, (GeckoSession)null, flags, (Map<String, String>) null);
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
        loadUri(uri, referrer, flags, (Map<String, String>) null);
    }

    /**
     * Load the given URI with the specified referrer, load type and HTTP request headers.
     *
     * @param uri the URI to load
     * @param referrer the referrer, may be null
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     * @param additionalHeaders any additional request headers used with the load
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @Nullable String referrer,
                        final @LoadFlags int flags, final @Nullable Map<String, String> additionalHeaders) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putString("uri", uri);
        msg.putInt("flags", flags);

        if (referrer != null) {
            msg.putString("referrerUri", referrer);
        }

        if (additionalHeaders != null) {
            msg.putStringArray("headers", additionalHeadersToStringArray(additionalHeaders));
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
        loadUri(uri, referrer, flags, (Map<String, String>) null);
    }

    /**
     * Load the given URI with the specified referrer, load type and HTTP request headers. This
     * method will also do any applicable checks to ensure that the specified URI is both safe and
     * allowable according to the referring GeckoSession.
     *
     * @param uri the URI to load
     * @param referrer the referring GeckoSession, may be null
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     * @param additionalHeaders any additional request headers used with the load
     */
    @AnyThread
    public void loadUri(final @NonNull String uri, final @Nullable GeckoSession referrer,
                        final @LoadFlags int flags, final @Nullable Map<String, String> additionalHeaders) {
        // For performance reasons we short-circuit the delegate here
        // instead of making Gecko call it for direct loadUri calls.
        final NavigationDelegate.LoadRequest request =
                new NavigationDelegate.LoadRequest(
                        uri,
                        null, /* triggerUri */
                        1, /* geckoTarget: OPEN_CURRENTWINDOW */
                        0, /* flags */
                        false, /* hasUserGesture */
                        true /* isDirectNavigation */);

        shouldLoadUri(request).getOrAccept(allowOrDeny -> {
            if (allowOrDeny == AllowOrDeny.DENY) {
                return;
            }

            final GeckoBundle msg = new GeckoBundle();
            msg.putString("uri", uri);
            msg.putInt("flags", flags);

            if (referrer != null) {
                msg.putString("referrerSessionId", referrer.mId);
            }

            if (additionalHeaders != null) {
                msg.putStringArray("headers", additionalHeadersToStringArray(additionalHeaders));
            }

            mEventDispatcher.dispatch("GeckoView:LoadUri", msg);
        });
    }

    private GeckoResult<AllowOrDeny> shouldLoadUri(final NavigationDelegate.LoadRequest request) {
        final NavigationDelegate delegate = mNavigationHandler.getDelegate();
        if (delegate == null) {
            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }

        final GeckoResult<AllowOrDeny> result = new GeckoResult<>();

        ThreadUtils.runOnUiThread(() -> {
            final GeckoResult<AllowOrDeny> delegateResult =
                    delegate.onLoadRequest(this, request);

            if (delegateResult == null) {
                result.complete(AllowOrDeny.ALLOW);
            } else {
                delegateResult.getOrAccept(
                    allowOrDeny -> result.complete(allowOrDeny),
                    error -> result.completeExceptionally(error)
                );
            }
        });

        return result;
    }

    /**
     * Load the given URI.
     * @param uri The URI of the resource to load.
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri) {
        loadUri(uri.toString(), (GeckoSession)null, LOAD_FLAGS_NONE, (Map<String, String>) null);
    }

    /**
     * Load the given URI with specified HTTP request headers.
     * @param uri The URI of the resource to load.
     * @param additionalHeaders any additional request headers used with the load
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri, final @Nullable Map<String, String> additionalHeaders) {
        loadUri(uri.toString(), (GeckoSession)null, LOAD_FLAGS_NONE, additionalHeaders);
    }

    /**
     * Load the given URI with the specified referrer and load type.
     * @param uri the URI to load
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri, final @LoadFlags int flags) {
        loadUri(uri.toString(), (GeckoSession)null, flags, (Map<String, String>) null);
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
        loadUri(uri.toString(), referrer != null ? referrer.toString() : null, flags, (Map<String, String>) null);
    }

    /**
     * Load the given URI with the specified referrer, load type and HTTP request headers.
     * @param uri the URI to load
     * @param referrer the Uri to use as the referrer
     * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
     * @param additionalHeaders any additional request headers used with the load
     */
    @AnyThread
    public void loadUri(final @NonNull Uri uri, final @Nullable Uri referrer,
                        final @LoadFlags int flags, final @Nullable Map<String, String> additionalHeaders) {
        loadUri(uri.toString(), referrer != null ? referrer.toString() : null, flags, additionalHeaders);
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
        reload(LOAD_FLAGS_NONE);
    }

    /**
    * Reload the current URI.
    *
    * @param flags the load flags to use, an OR-ed value of {@link #LOAD_FLAGS_NONE LOAD_FLAGS_*}
    */
    @AnyThread
    public void reload(final @LoadFlags int flags) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putInt("flags", flags);
        mEventDispatcher.dispatch("GeckoView:Reload", msg);
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

    /**
     * Returns a WebExtensionController for this GeckoSession. Delegates attached
     * to this controller will receive events specific to this session.
     *
     * @return an instance of {@link WebExtension.SessionController}.
     */
    @UiThread
    public @NonNull WebExtension.SessionController getWebExtensionController() {
        return mWebExtensionController;
    }

    /**
     * Purge history for the session.
     * The session history is used for back and forward history.
     * Purging the session history means {@link NavigationDelegate#onCanGoBack(GeckoSession, boolean)}
     * and {@link NavigationDelegate#onCanGoForward(GeckoSession, boolean)} will be false.
     */
    @AnyThread
    public void purgeHistory() {
        mEventDispatcher.dispatch("GeckoView:PurgeHistory", null);
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

        ThreadUtils.runOnUiThread(
            () -> getAutofillSupport().onActiveChanged(active)
        );
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
        mProcessHangHandler.setDelegate(delegate, this);
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
        final String title = message.getString("title");
        final String msg = message.getString("msg");
        GeckoResult<PromptDelegate.PromptResponse> res = null;

        switch (type) {
            case "alert": {
                final PromptDelegate.AlertPrompt prompt =
                    new PromptDelegate.AlertPrompt(title, msg);
                res = delegate.onAlertPrompt(session, prompt);
                break;
            }
            case "button": {
                final PromptDelegate.ButtonPrompt prompt =
                    new PromptDelegate.ButtonPrompt(title, msg);
                res = delegate.onButtonPrompt(session, prompt);
                break;
            }
            case "text": {
                final String defaultValue = message.getString("value");
                final PromptDelegate.TextPrompt prompt =
                    new PromptDelegate.TextPrompt(title, msg, defaultValue);
                res = delegate.onTextPrompt(session, prompt);
                break;
            }
            case "auth": {
                final PromptDelegate.AuthPrompt.AuthOptions authOptions =
                    new PromptDelegate.AuthPrompt.AuthOptions(message.getBundle("options"));
                final PromptDelegate.AuthPrompt prompt =
                    new PromptDelegate.AuthPrompt(title, msg, authOptions);
                res = delegate.onAuthPrompt(session, prompt);
                break;
            }
            case "choice": {
                final int intMode;
                if ("menu".equals(mode)) {
                    intMode = PromptDelegate.ChoicePrompt.Type.MENU;
                } else if ("single".equals(mode)) {
                    intMode = PromptDelegate.ChoicePrompt.Type.SINGLE;
                } else if ("multiple".equals(mode)) {
                    intMode = PromptDelegate.ChoicePrompt.Type.MULTIPLE;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }

                GeckoBundle[] choiceBundles = message.getBundleArray("choices");
                PromptDelegate.ChoicePrompt.Choice choices[];
                if (choiceBundles == null || choiceBundles.length == 0) {
                    choices = new PromptDelegate.ChoicePrompt.Choice[0];
                } else {
                    choices = new PromptDelegate.ChoicePrompt.Choice[choiceBundles.length];
                    for (int i = 0; i < choiceBundles.length; i++) {
                        choices[i] = new PromptDelegate.ChoicePrompt.Choice(choiceBundles[i]);
                    }
                }

                final PromptDelegate.ChoicePrompt prompt =
                    new PromptDelegate.ChoicePrompt(title, msg, intMode, choices);
                res = delegate.onChoicePrompt(session, prompt);
                break;
            }
            case "color": {
                final String defaultValue = message.getString("value");
                final PromptDelegate.ColorPrompt prompt =
                    new PromptDelegate.ColorPrompt(title, defaultValue);
                res = delegate.onColorPrompt(session, prompt);
                break;
            }
            case "datetime": {
                final int intMode;
                if ("date".equals(mode)) {
                    intMode = PromptDelegate.DateTimePrompt.Type.DATE;
                } else if ("month".equals(mode)) {
                    intMode = PromptDelegate.DateTimePrompt.Type.MONTH;
                } else if ("week".equals(mode)) {
                    intMode = PromptDelegate.DateTimePrompt.Type.WEEK;
                } else if ("time".equals(mode)) {
                    intMode = PromptDelegate.DateTimePrompt.Type.TIME;
                } else if ("datetime-local".equals(mode)) {
                    intMode = PromptDelegate.DateTimePrompt.Type.DATETIME_LOCAL;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }

                final String defaultValue = message.getString("value");
                final String minValue = message.getString("min");
                final String maxValue = message.getString("max");
                final PromptDelegate.DateTimePrompt prompt =
                    new PromptDelegate.DateTimePrompt(title, intMode, defaultValue, minValue, maxValue);
                res = delegate.onDateTimePrompt(session, prompt);
                break;
            }
            case "file": {
                final int intMode;
                if ("single".equals(mode)) {
                    intMode = PromptDelegate.FilePrompt.Type.SINGLE;
                } else if ("multiple".equals(mode)) {
                    intMode = PromptDelegate.FilePrompt.Type.MULTIPLE;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }

                String[] mimeTypes = message.getStringArray("mimeTypes");
                int capture = message.getInt("capture");
                final PromptDelegate.FilePrompt prompt =
                    new PromptDelegate.FilePrompt(title, intMode, capture, mimeTypes);
                res = delegate.onFilePrompt(session, prompt);
                break;
            }
            case "popup": {
                final String targetUri = message.getString("targetUri");
                final PromptDelegate.PopupPrompt prompt =
                    new PromptDelegate.PopupPrompt(targetUri);
                res = delegate.onPopupPrompt(session, prompt);
                break;
            }
            case "share": {
                final String text = message.getString("text");
                final String uri = message.getString("uri");
                final PromptDelegate.SharePrompt prompt =
                    new PromptDelegate.SharePrompt(title, text, uri);
                res = delegate.onSharePrompt(session, prompt);
                break;
            }
            case "loginStorage": {
                final int lsType = message.getInt("lsType");
                final int hint = message.getInt("hint");
                final GeckoBundle[] loginBundles =
                    message.getBundleArray("logins");

                if (loginBundles == null) {
                    break;
                }

                final LoginStorage.LoginEntry[] logins =
                    new LoginStorage.LoginEntry[loginBundles.length];

                for (int i = 0; i < logins.length; ++i) {
                    logins[i] = new LoginStorage.LoginEntry(loginBundles[i]);
                }

                final PromptDelegate.LoginStoragePrompt prompt =
                    new PromptDelegate.LoginStoragePrompt(lsType, hint, logins);

                res = delegate.onLoginStoragePrompt(session, prompt);
                break;
            }
            default: {
                callback.sendError("Invalid type");
                return;
            }
        }

        if (res == null) {
            // Adhere to default behavior if the delegate returns null.
            callback.sendSuccess(null);
        } else {
            res.accept(value -> {
                value.dispatch(callback);
            }, exception -> callback.sendError("Failed to get prompt response."));
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
             * The server certificate in use, if any.
             */
            public final @Nullable X509Certificate certificate;

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

                X509Certificate decodedCert = null;
                try {
                    final CertificateFactory factory = CertificateFactory.getInstance("X.509");
                    final String certString = identityData.getString("certificate");
                    if (certString != null) {
                        final byte[] certBytes = Base64.decode(certString, Base64.NO_WRAP);
                        decodedCert = (X509Certificate) factory.generateCertificate(new ByteArrayInputStream(certBytes));
                    }
                } catch (CertificateException e) {
                    Log.e(LOGTAG, "Failed to decode certificate", e);
                }

                certificate = decodedCert;
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
                certificate = null;
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
         * A viewport-fit was discovered in the content or updated after the content.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param viewportFit The value of viewport-fit of meta element in content.
         * @see <a href="https://drafts.csswg.org/css-round-display/#viewport-fit-descriptor">4.1. The viewport-fit descriptor</a>
         */
        @UiThread
        default void onMetaViewportFitChange(@NonNull GeckoSession session, @NonNull String viewportFit) {}

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
         * Notification that the first content paint has occurred.
         * This callback is invoked for the first content paint after
         * a page has been loaded. The function {@link #onFirstComposite(GeckoSession)}
         * will be called once the compositor has started rendering. However, it is
         * possible for the compositor to start rendering before there is any content to render.
         * onFirstContentfulPaint() is called once some content has been rendered. It may be nothing
         * more than the page background color. It is not an indication that the whole page has
         * been rendered.
         * @param session The GeckoSession that had a first paint event.
         */
        @UiThread
        default void onFirstContentfulPaint(@NonNull GeckoSession session) {}

        /**
         * This is fired when the loaded document has a valid Web App Manifest present.
         *
         * The various colors (theme_color, background_color, etc.) present in the manifest
         * have been  transformed into #AARRGGBB format.
         *
         * @param session The GeckoSession that contains the Web App Manifest
         * @param manifest A parsed and validated {@link JSONObject} containing the manifest contents.
         * @see <a href="https://www.w3.org/TR/appmanifest/">Web App Manifest specification</a>
         */
        @UiThread
        default void onWebAppManifest(@NonNull GeckoSession session, @NonNull JSONObject manifest) {}

        /**
         * A script has exceeded it's execution timeout value
         * @param geckoSession GeckoSession that initiated the callback.
         * @param scriptFileName Filename of the slow script
         * @return A {@link GeckoResult} with a SlowScriptResponse value which indicates whether to
         *         allow the Slow Script to continue processing. Stop will halt the slow script.
         *         Continue will pause notifications for a period of time before resuming.
         */
        @UiThread
        default @Nullable GeckoResult<SlowScriptResponse> onSlowScript(@NonNull GeckoSession geckoSession,
                                                                       @NonNull String scriptFileName) {
            return null;
        }
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

            /**
             * Set of valid actions available through {@link Selection#execute(String)}
             */
            public final @NonNull @SelectionActionDelegateAction Collection<String> availableActions;

            private final int mSeqNo;

            private final EventCallback mEventCallback;

            /* package */ Selection(final GeckoBundle bundle,
                                    final @NonNull @SelectionActionDelegateAction Set<String> actions,
                                    final EventCallback callback) {
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

                availableActions = actions;
                mSeqNo = bundle.getInt("seqNo");
                mEventCallback = callback;
            }

            /**
             * Empty constructor for tests.
             */
            protected Selection() {
                flags = 0;
                text = "";
                clientRect = null;
                availableActions = new HashSet<>();
                mSeqNo = 0;
                mEventCallback = null;
            }

            /**
             * Checks if the passed action is available
             * @param action An {@link SelectionActionDelegateAction} to perform
             * @return True if the action is available.
             */
            @AnyThread
            public boolean isActionAvailable(@NonNull @SelectionActionDelegateAction final String action) {
                return availableActions.contains(action);
            }

            /**
             * Execute an {@link SelectionActionDelegateAction} action.
             *
             * @throws IllegalStateException If the action was not available.
             * @param action A {@link SelectionActionDelegateAction} action.
             */
            @AnyThread
            public void execute(@NonNull @SelectionActionDelegateAction final String action) {
                if (!isActionAvailable(action)) {
                    throw new IllegalStateException("Action not available");
                }
                final GeckoBundle response = new GeckoBundle(2);
                response.putString("id", action);
                response.putInt("seqNo", mSeqNo);
                mEventCallback.sendSuccess(response);
            }

            /**
             * Hide selection actions and cause {@link #onHideAction} to be called.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void hide() {
                execute(ACTION_HIDE);
            }

            /**
             * Copy onto the clipboard then delete the selected content.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void cut() {
                execute(ACTION_CUT);
            }

            /**
             * Copy the selected content onto the clipboard.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void copy() {
                execute(ACTION_COPY);
            }

            /**
             * Delete the selected content.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void delete() {
                execute(ACTION_DELETE);
            }

            /**
             * Replace the selected content with the clipboard content.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void paste() {
                execute(ACTION_PASTE);
            }

            /**
             * Select the entire content of the document or editor.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void selectAll() {
                execute(ACTION_SELECT_ALL);
            }

            /**
             * Clear the current selection.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void unselect() {
                execute(ACTION_UNSELECT);
            }

            /**
             * Collapse the current selection to its start position.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void collapseToStart() {
                execute(ACTION_COLLAPSE_TO_START);
            }

            /**
             * Collapse the current selection to its end position.
             *
             * @throws IllegalStateException If the action was not available.
             */
            @AnyThread
            public void collapseToEnd() {
                execute(ACTION_COLLAPSE_TO_END);
            }
        }

        /**
         * Selection actions are available. Selection actions become available when the
         * user selects some content in the document or editor. Inside an editor,
         * selection actions can also become available when the user explicitly requests
         * editor action UI, for example by tapping on the caret handle.
         *
         * In response to this callback, applications typically display a toolbar
         * containing the selection actions. To perform a certain action, check if the action
         * is available with {@link Selection#isActionAvailable} then either use the relevant
         * helper method or {@link Selection#execute}
         *
         * Once an {@link #onHideAction} call (with particular reasons) or another {@link
         * #onShowActionRequest} call is received, the previous Selection object is no longer
         * usable.
         *
         * @param session The GeckoSession that initiated the callback.
         * @param selection Current selection attributes and Callback object for performing built-in
         *                  actions. May be used multiple times to perform multiple actions at once.
         */
        @UiThread
        default void onShowActionRequest(@NonNull GeckoSession session,
                                         @NonNull Selection selection) {}

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
                                      final int flags,
                                      final boolean hasUserGesture,
                                      final boolean isDirectNavigation) {
                this.uri = uri;
                this.triggerUri = triggerUri;
                this.target = convertGeckoTarget(geckoTarget);
                this.isRedirect = (flags & LOAD_REQUEST_IS_REDIRECT) != 0;
                this.hasUserGesture = hasUserGesture;
                this.isDirectNavigation = isDirectNavigation;
            }

            /**
             * Empty constructor for tests.
             */
            protected LoadRequest() {
                uri = "";
                triggerUri = null;
                target = 0;
                isRedirect = false;
                hasUserGesture = false;
                isDirectNavigation = false;
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

            /**
             * True if there was an active user gesture when the load was requested.
             */
            public final boolean hasUserGesture;

            /**
             * This load request was initiated by a direct navigation from the
             * application. E.g. when calling {@link GeckoSession#loadUri}.
             */
            public final boolean isDirectNavigation;

            @Override
            public String toString() {
                final StringBuilder out = new StringBuilder("LoadRequest { ");
                out
                    .append("uri: " + uri)
                    .append(", triggerUri: " + triggerUri)
                    .append(", target: " + target)
                    .append(", isRedirect: " + isRedirect)
                    .append(", hasUserGesture: " + hasUserGesture)
                    .append(", fromLoadUri: " + hasUserGesture)
                    .append(" }");
                return out.toString();
            }
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
         * @return A {@link GeckoResult} with a {@link AllowOrDeny} value which indicates whether
         *         or not the load was handled. If unhandled, Gecko will continue the
         *         load as normal. If handled (a {@link AllowOrDeny#DENY DENY} value), Gecko
         *         will abandon the load. A null return value is interpreted as
         *         {@link AllowOrDeny#ALLOW ALLOW} (unhandled).
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
         *        The implementation of onNewSession is responsible for maintaining a reference
         *        to the returned object, to prevent it from being garbage collected.
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
         *         The following special methods are made available to the URI:
         *         - document.addCertException(isTemporary), returns Promise
         *         - document.getFailedCertSecurityInfo(), returns FailedCertSecurityInfo
         *         - document.getNetErrorInfo(), returns NetErrorInfo
         * @see <a href="https://searchfox.org/mozilla-central/source/dom/webidl/FailedCertSecurityInfo.webidl">FailedCertSecurityInfo IDL</a>
         * @see <a href="https://searchfox.org/mozilla-central/source/dom/webidl/NetErrorInfo.webidl">NetErrorInfo IDL</a>
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
         * PromptResponse is an opaque class created upon confirming or dismissing a
         * prompt.
         */
        public class PromptResponse {
            private final BasePrompt mPrompt;

            /* package */ PromptResponse(@NonNull final BasePrompt prompt) {
                mPrompt = prompt;
            }

            /* package */ void dispatch(@NonNull final EventCallback callback) {
                if (mPrompt == null) {
                    throw new RuntimeException("Trying to confirm/dismiss a null prompt.");
                }
                mPrompt.dispatch(callback);
            }
        }

        // Prompt classes.
        public class BasePrompt {
            private boolean mIsCompleted;
            private boolean mIsConfirmed;
            private GeckoBundle mResult;

            /**
             * The title of this prompt; may be null.
             */
            public final @Nullable String title;

            private BasePrompt(@Nullable final String title) {
                this.title = title;
                mIsConfirmed = false;
                mIsCompleted = false;
            }

            @UiThread
            protected @NonNull PromptResponse confirm() {
                if (mIsCompleted) {
                    throw new RuntimeException("Cannot confirm/dismiss a Prompt twice.");
                }

                mIsCompleted = true;
                mIsConfirmed = true;
                return new PromptResponse(this);
            }

            /**
             * This dismisses the prompt without sending any meaningful information back
             * to content.
             *
             * @return A {@link PromptResponse} with which you can complete the
             *         {@link GeckoResult} that corresponds to this prompt.
             */
            @UiThread
            public @NonNull PromptResponse dismiss() {
                if (mIsCompleted) {
                    throw new RuntimeException("Cannot confirm/dismiss a Prompt twice.");
                }

                mIsCompleted = true;
                return new PromptResponse(this);
            }

            /* package */ GeckoBundle ensureResult() {
                if (mResult == null) {
                    // Usually result object contains two items.
                    mResult = new GeckoBundle(2);
                }
                return mResult;
            }

            /**
             * This returns true if the prompt has already been confirmed or dismissed.
             *
             * @return A boolean which is true if the prompt has been confirmed or dismissed,
             *         and false otherwise.
             */
            @UiThread
            public boolean isComplete() {
                return mIsCompleted;
            }

            /* package */ void dispatch(@NonNull final EventCallback callback) {
                if (!mIsCompleted) {
                    throw new RuntimeException("Trying to dispatch an incomplete prompt.");
                }

                if (!mIsConfirmed) {
                    callback.sendSuccess(null);
                } else {
                    callback.sendSuccess(mResult);
                }
            }
        }

        /**
         * AlertPrompt contains the information necessary to represent a JavaScript
         * alert() call from content; it can only be dismissed, not confirmed.
         */
        public class AlertPrompt extends BasePrompt {
            /**
             * The message to be displayed with this alert; may be null.
             */
            public final @Nullable String message;

            protected AlertPrompt(@Nullable final String title,
                                  @Nullable final String message) {
                super(title);
                this.message = message;
            }
        }

        /**
         * ButtonPrompt contains the information necessary to represent a JavaScript
         * confirm() call from content.
         */
        public class ButtonPrompt extends BasePrompt {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Type.POSITIVE, Type.NEGATIVE})
            /* package */ @interface ButtonType {}

            public static class Type {
                /**
                 * Index of positive response button (eg, "Yes", "OK")
                 */
                public static final int POSITIVE = 0;

                /**
                 * Index of negative response button (eg, "No", "Cancel")
                 */
                public static final int NEGATIVE = 2;

                protected Type() {}
            }

            /**
             * The message to be displayed with this prompt; may be null.
             */
            public final @Nullable String message;

            protected ButtonPrompt(@Nullable final String title,
                                   @Nullable final String message) {
                super(title);
                this.message = message;
            }

            /**
             * Confirms this prompt, returning the selected button to content.
             *
             * @param selection An int representing the selected button, must be
             *                  one of {@link Type}.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@ButtonType final int selection) {
                ensureResult().putInt("button", selection);
                return super.confirm();
            }
        }

        /**
         * TextPrompt contains the information necessary to represent a Javascript
         * prompt() call from content.
         */
        public class TextPrompt extends BasePrompt {
            /**
             * The message to be displayed with this prompt; may be null.
             */
            public final @Nullable String message;

            /**
             * The default value for the text field; may be null.
             */
            public final @Nullable String defaultValue;

            protected TextPrompt(@Nullable final String title,
                                 @Nullable final String message,
                                 @Nullable final String defaultValue) {
                super(title);
                this.message = message;
                this.defaultValue = defaultValue;
            }

            /**
             * Confirms this prompt, returning the input text to content.
             *
             * @param text A String containing the text input given by the user.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String text) {
                ensureResult().putString("text", text);
                return super.confirm();
            }
        }

        /**
         * AuthPrompt contains the information necessary to represent an HTML
         * authorization prompt generated by content.
         */
        public class AuthPrompt extends BasePrompt {
            public static class AuthOptions {
                @Retention(RetentionPolicy.SOURCE)
                @IntDef(flag = true,
                        value = {Flags.HOST, Flags.PROXY, Flags.ONLY_PASSWORD,
                                 Flags.PREVIOUS_FAILED, Flags.CROSS_ORIGIN_SUB_RESOURCE})
                /* package */ @interface AuthFlag {}

                /**
                 * Auth prompt flags.
                 */
                public static class Flags {
                    /**
                     * The auth prompt is for a network host.
                     */
                    public static final int HOST = 1;
                    /**
                     * The auth prompt is for a proxy.
                     */
                    public static final int PROXY = 2;
                    /**
                     * The auth prompt should only request a password.
                     */
                    public static final int ONLY_PASSWORD = 8;
                    /**
                     * The auth prompt is the result of a previous failed login.
                     */
                    public static final int PREVIOUS_FAILED = 16;
                    /**
                     * The auth prompt is for a cross-origin sub-resource.
                     */
                    public static final int CROSS_ORIGIN_SUB_RESOURCE = 32;

                    protected Flags() {}
                }

                @Retention(RetentionPolicy.SOURCE)
                @IntDef({Level.NONE, Level.PW_ENCRYPTED, Level.SECURE})
                /* package */ @interface AuthLevel {}

                /**
                 * Auth prompt levels.
                 */
                public static class Level {
                    /**
                     * The auth request is unencrypted or the encryption status is unknown.
                     */
                    public static final int NONE = 0;
                    /**
                     * The auth request only encrypts password but not data.
                     */
                    public static final int PW_ENCRYPTED = 1;
                    /**
                     * The auth request encrypts both password and data.
                     */
                    public static final int SECURE = 2;

                    protected Level() {}
                }

                /**
                 * An int bit-field of {@link Flags}.
                 */
                public @AuthFlag final int flags;

                /**
                 * A string containing the URI for the auth request or null if unknown.
                 */
                public @Nullable final String uri;

                /**
                 * An int, one of {@link Level}, indicating level of encryption.
                 */
                public @AuthLevel final int level;

                /**
                 * A string containing the initial username or null if password-only.
                 */
                public @Nullable final String username;

                /**
                 * A string containing the initial password.
                 */
                public @Nullable final String password;

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
             * The message to be displayed with this prompt; may be null.
             */
            public final @Nullable String message;

            /**
             * The {@link AuthOptions} that describe the type of authorization prompt.
             */
            public final @NonNull AuthOptions authOptions;

            protected AuthPrompt(@Nullable final String title,
                                 @Nullable final String message,
                                 @NonNull final AuthOptions authOptions) {
                super(title);
                this.message = message;
                this.authOptions = authOptions;
            }

            /**
             * Confirms this prompt with just a password, returning the password to content.
             *
             * @param password A String containing the password input by the user.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String password) {
                ensureResult().putString("password", password);
                return super.confirm();
            }

            /**
             * Confirms this prompt with a username and password, returning both to content.
             *
             * @param username A String containing the username input by the user.
             * @param password A String containing the password input by the user.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String username,
                                                   @NonNull final String password) {
                ensureResult().putString("username", username);
                ensureResult().putString("password", password);
                return super.confirm();
            }
        }

        /**
         * ChoicePrompt contains the information necessary to display a menu or list prompt
         * generated by content.
         */
        public class ChoicePrompt extends BasePrompt {
            public static class Choice {
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

            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Type.MENU, Type.SINGLE, Type.MULTIPLE})
            /* package */ @interface ChoiceType {}

            public static class Type {
                /**
                 * Display choices in a menu that dismisses as soon as an item is chosen.
                 */
                public static final int MENU = 1;

                /**
                 * Display choices in a list that allows a single selection.
                 */
                public static final int SINGLE = 2;

                /**
                 * Display choices in a list that allows multiple selections.
                 */
                public static final int MULTIPLE = 3;

                protected Type() {}
            }

            /**
             * The message to be displayed with this prompt; may be null.
             */
            public final @Nullable String message;

            /**
             * One of {@link Type}.
             */
            public final @ChoiceType int type;

            /**
             * An array of {@link Choice} representing possible choices.
             */
            public final @NonNull Choice[] choices;

            protected ChoicePrompt(@Nullable final String title,
                                   @Nullable final String message,
                                   @ChoiceType final int type,
                                   @NonNull final Choice[] choices) {
                super(title);
                this.message = message;
                this.type = type;
                this.choices = choices;
            }

            /**
             * Confirms this prompt with the string id of a single choice.
             *
             * @param selectedId The string ID of the selected choice.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String selectedId) {
                return confirm(new String[] { selectedId });
            }

            /**
             * Confirms this prompt with the string ids of multiple choices
             *
             * @param selectedIds The string IDs of the selected choices.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String[] selectedIds) {
                if ((Type.MENU == type || Type.SINGLE == type) &&
                    (selectedIds == null || selectedIds.length != 1)) {
                    throw new IllegalArgumentException();
                }
                ensureResult().putStringArray("choices", selectedIds);
                return super.confirm();
            }

            /**
             * Confirms this prompt with a single choice.
             *
             * @param selectedChoice The selected choice.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final Choice selectedChoice) {
                return confirm(selectedChoice == null ? null : selectedChoice.id);
            }

            /**
             * Confirms this prompt with multiple choices.
             *
             * @param selectedChoices The selected choices.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final Choice[] selectedChoices) {
                if ((Type.MENU == type || Type.SINGLE == type) &&
                    (selectedChoices == null || selectedChoices.length != 1)) {
                    throw new IllegalArgumentException();
                }

                if (selectedChoices == null) {
                    return confirm((String[]) null);
                }

                final String[] ids = new String[selectedChoices.length];
                for (int i = 0; i < ids.length; i++) {
                    ids[i] = (selectedChoices[i] == null) ? null : selectedChoices[i].id;
                }

                return confirm(ids);
            }
        }

        /**
         * ColorPrompt contains the information necessary to represent a prompt for color
         * input generated by content.
         */
        public class ColorPrompt extends BasePrompt {
            /**
             * The default value supplied by content.
             */
            public final @Nullable String defaultValue;

            protected ColorPrompt(@Nullable final String title,
                                  @Nullable final String defaultValue) {
                super(title);
                this.defaultValue = defaultValue;
            }

            /**
             * Confirms the prompt and passes the color value back to content.
             *
             * @param color A String representing the color to be returned to content.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String color) {
                ensureResult().putString("color", color);
                return super.confirm();
            }
        }

        /**
         * DateTimePrompt contains the information necessary to represent a prompt for
         * date and/or time input generated by content.
         */
        public class DateTimePrompt extends BasePrompt {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Type.DATE, Type.MONTH, Type.WEEK, Type.TIME, Type.DATETIME_LOCAL})
            /* package */ @interface DatetimeType {}

            public static class Type {
                /**
                 * Prompt for year, month, and day.
                 */
                public static final int DATE = 1;

                /**
                 * Prompt for year and month.
                 */
                public static final int MONTH = 2;

                /**
                 * Prompt for year and week.
                 */
                public static final int WEEK = 3;

                /**
                 * Prompt for hour and minute.
                 */
                public static final int TIME = 4;

                /**
                 * Prompt for year, month, day, hour, and minute, without timezone.
                 */
                public static final int DATETIME_LOCAL = 5;

                protected Type() {}
            }

            /**
             * One of {@link Type} indicating the type of prompt.
             */
            public final @DatetimeType int type;

            /**
             * A String representing the default value supplied by content.
             */
            public final @Nullable String defaultValue;

            /**
             * A String representing the minimum value allowed by content.
             */
            public final @Nullable String minValue;

            /**
             * A String representing the maximum value allowed by content.
             */
            public final @Nullable String maxValue;

            protected DateTimePrompt(@Nullable final String title,
                                     @DatetimeType final int type,
                                     @Nullable final String defaultValue,
                                     @Nullable final String minValue,
                                     @Nullable final String maxValue) {
                super(title);
                this.type = type;
                this.defaultValue = defaultValue;
                this.minValue = minValue;
                this.maxValue = maxValue;
            }

            /**
             * Confirms the prompt and passes the date and/or time value back to content.
             *
             * @param datetime A String representing the date and time to be returned to content.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final String datetime) {
                ensureResult().putString("datetime", datetime);
                return super.confirm();
            }
        }

        /**
         * FilePrompt contains the information necessary to represent a prompt for
         * a file or files generated by content.
         */
        public class FilePrompt extends BasePrompt {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Type.SINGLE, Type.MULTIPLE})
            /* package */ @interface FileType {}

            /**
             * Types of file prompts.
             */
            public static class Type {
                /**
                 * Prompt for a single file.
                 */
                public static final int SINGLE = 1;

                /**
                 * Prompt for multiple files.
                 */
                public static final int MULTIPLE = 2;

                protected Type() {}
            }

            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Capture.NONE, Capture.ANY, Capture.USER, Capture.ENVIRONMENT})
            /* package */ @interface CaptureType {}

            /**
             * Possible capture attribute values.
             */
            public static class Capture {
                // These values should match the corresponding values in nsIFilePicker.idl
                /**
                 * No capture attribute has been supplied by content.
                 */
                public static final int NONE = 0;

                /**
                 * The capture attribute was supplied with a missing or invalid value.
                 */
                public static final int ANY = 1;

                /**
                 * The "user" capture attribute has been supplied by content.
                 */
                public static final int USER = 2;

                /**
                 * The "environment" capture attribute has been supplied by content.
                 */
                public static final int ENVIRONMENT = 3;

                protected Capture() {}
            }

            /**
             * One of {@link Type} indicating the prompt type.
             */
            public final @FileType int type;

            /**
             * An array of Strings giving the MIME types specified by the "accept" attribute,
             * if any are specified.
             */
            public final @Nullable String[] mimeTypes;

            /**
             * One of {@link Capture} indicating the capture attribute supplied by content.
             */
            public final @CaptureType int capture;

            protected FilePrompt(@Nullable final String title,
                                 @FileType final int type,
                                 @CaptureType final int capture,
                                 @Nullable final String[] mimeTypes) {
                super(title);
                this.type = type;
                this.capture = capture;
                this.mimeTypes = mimeTypes;
            }

            /**
             * Confirms the prompt and passes the file URI back to content.
             *
             * @param context An Application context for parsing URIs.
             * @param uri The URI of the file chosen by the user.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final Context context,
                                                   @NonNull final Uri uri) {
                return confirm(context, new Uri[] { uri });
            }

            /**
             * Confirms the prompt and passes the file URIs back to content.
             *
             * @param context An Application context for parsing URIs.
             * @param uris The URIs of the files chosen by the user.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final Context context,
                                                   @NonNull final Uri[] uris) {
                if (Type.SINGLE == type && (uris == null || uris.length != 1)) {
                    throw new IllegalArgumentException();
                }

                final String[] paths = new String[uris != null ? uris.length : 0];
                for (int i = 0; i < paths.length; i++) {
                    paths[i] = getFile(context, uris[i]);
                    if (paths[i] == null) {
                        Log.e(LOGTAG, "Only file URIs are supported: " + uris[i]);
                    }
                }
                ensureResult().putStringArray("files", paths);

                return super.confirm();
            }

            private static String getFile(final @NonNull Context context, final @NonNull Uri uri) {
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
        }

        /**
         * PopupPrompt contains the information necessary to represent a popup blocking
         * request.
         */
        public class PopupPrompt extends BasePrompt {
            /**
             * The target URI for the popup; may be null.
             */
            public final @Nullable String targetUri;

            protected PopupPrompt(@Nullable final String targetUri) {
                super(null);
                this.targetUri = targetUri;
            }

            /**
             * Confirms the prompt and either allows or blocks the popup.
             *
             * @param response An {@link AllowOrDeny} specifying whether to allow or deny the popup.
             *
             * @return A {@link PromptResponse} which can be used to complete
             *         the {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@NonNull final AllowOrDeny response) {
                boolean res = false;
                if (AllowOrDeny.ALLOW == response) {
                    res = true;
                }
                ensureResult().putBoolean("response", res);
                return super.confirm();
            }
        }

        /**
         * SharePrompt contains the information necessary to represent a (v1) WebShare request.
         */
        public class SharePrompt extends BasePrompt {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({Result.SUCCESS, Result.FAILURE, Result.ABORT})
            /* package */ @interface ShareResult {}

            /**
             * Possible results to a {@link SharePrompt}.
             */
            public static class Result {
                /**
                 * The user shared with another app successfully.
                 */
                public static final int SUCCESS = 0;

                /**
                 * The user attempted to share with another app, but it failed.
                 */
                public static final int FAILURE = 1;

                /**
                 * The user aborted the share.
                 */
                public static final int ABORT = 2;

                protected Result() {}
            }

            /**
             * The text for the share request.
             */
            public final @Nullable String text;

            /**
             * The uri for the share request.
             */
            public final @Nullable String uri;

            protected SharePrompt(@Nullable final String title,
                                  @Nullable final String text,
                                  @Nullable final String uri) {
                super(title);
                this.text = text;
                this.uri = uri;
            }

            /**
             * Confirms the prompt and either blocks or allows the share request.
             *
             * @param response One of {@link Result} specifying the outcome of the
             *                 share attempt.
             *
             * @return A {@link PromptResponse} which can be used to complete the
             *         {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(@ShareResult final int response) {
                ensureResult().putInt("response", response);
                return super.confirm();
            }

            /**
             * Dismisses the prompt and returns {@link Result#ABORT} to web content.
             *
             * @return A {@link PromptResponse} which can be used to complete the
             *         {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse dismiss() {
                ensureResult().putInt("response", Result.ABORT);
                return super.dismiss();
            }
        }

        /**
         * LoginStoragePrompt contains the information necessary to handle a
         * login storage request.
         */
        public class LoginStoragePrompt extends BasePrompt {
            @Retention(RetentionPolicy.SOURCE)
            @IntDef({ Type.SAVE })
            /* package */ @interface LoginStorageType {}

            // Sync with LoginStorageDelegate.Type in GeckoViewPrompt.js
            /**
             * Possible type of a {@link LoginStoragePrompt}.
             */
            public static class Type {
                public static final int SAVE = 1;

                protected Type() {}
            }

            @Retention(RetentionPolicy.SOURCE)
            @IntDef(flag = true,
                    value = { Hint.NONE })
            /* package */ @interface LoginStorageHint {}

            public static class Hint {
                public static final int NONE = 0;

                protected Hint() {}
            }

            /**
             * The type of the prompt request, one of {@link LoginStoragePrompt.Type}.
             */
            public final @LoginStorageType int type;

            /**
             * The hint may provide some additional information on the nature or
             * confidence level of the prompt request to support appropriate
             * prompting styles. A flag combination of {@link LoginStoragePrompt.Hint}.
             */
            public final @LoginStorageHint int hint;

            /**
             * The logins that are subject to the prompt request.
             * For {@link Type#SAVE}, it holds one
             * {@link LoginStorage.LoginEntry} depicting the entry to be saved.
             */
            public final @NonNull LoginStorage.LoginEntry[] logins;

            protected LoginStoragePrompt(
                    final @LoginStorageType int type,
                    final @LoginStorageHint int hint,
                    final @NonNull LoginStorage.LoginEntry[] logins) {
                super(null);

                this.type = type;
                this.hint = hint;
                this.logins = logins;
            }

            /**
             * Confirm the prompt by responding with a
             * {@link LoginStorage.LoginEntry}.
             * For {@link Type#SAVE}, confirm with the entry to be saved. This
             * can be the original entry as provided by logins[0] or a modified
             * entry based on that.
             * Confirming a {@link Type#SAVE} prompt request triggers a
             * {@link LoginStorage.Delegate#onLoginSave} request in the runtime
             * delegate with the confirmed login entry.
             *
             * @param login A {@link LoginStorage.LoginEntry} specifying the
             *              login entry saved.
             *
             * @return A {@link PromptResponse} which can be used to complete the
             *         {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse confirm(
                    final @NonNull LoginStorage.LoginEntry login) {
                ensureResult().putBundle("login", login.toBundle());
                return super.confirm();
            }

            /**
             * Dismisses the prompt.
             *
             * @return A {@link PromptResponse} which can be used to complete the
             *         {@link GeckoResult} associated with this prompt.
             */
            @UiThread
            public @NonNull PromptResponse dismiss() {
                return super.dismiss();
            }
        }

        // Delegate functions.
        /**
         * Display an alert prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link AlertPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onAlertPrompt(@NonNull final GeckoSession session,
                                                                    @NonNull final AlertPrompt prompt) {
            return null;
        }

        /**
         * Display a button prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link ButtonPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onButtonPrompt(@NonNull final GeckoSession session,
                                                                     @NonNull final ButtonPrompt prompt) {
            return null;
        }

        /**
         * Display a text prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link TextPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onTextPrompt(@NonNull final GeckoSession session,
                                                                   @NonNull final TextPrompt prompt) {
            return null;
        }

        /**
         * Display an authorization prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link AuthPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onAuthPrompt(@NonNull final GeckoSession session,
                                                                   @NonNull final AuthPrompt prompt) {
            return null;
        }

        /**
         * Display a list/menu prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link ChoicePrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onChoicePrompt(@NonNull final GeckoSession session,
                                                                     @NonNull final ChoicePrompt prompt) {
            return null;
        }

        /**
         * Display a color prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link ColorPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onColorPrompt(@NonNull final GeckoSession session,
                                                                    @NonNull final ColorPrompt prompt) {
            return null;
        }

        /**
         * Display a date/time prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link DateTimePrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onDateTimePrompt(@NonNull final GeckoSession session,
                                                                       @NonNull final DateTimePrompt prompt) {
            return null;
        }

        /**
         * Display a file prompt.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link FilePrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onFilePrompt(@NonNull final GeckoSession session,
                                                                   @NonNull final FilePrompt prompt) {
            return null;
        }

        /**
         * Display a popup request prompt; this occurs when content attempts to open
         * a new window in a way that doesn't appear to be the result of user input.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link PopupPrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onPopupPrompt(@NonNull final GeckoSession session,
                                                                    @NonNull final PopupPrompt prompt) {
            return null;
        }

        /**
         * Display a share request prompt; this occurs when content attempts to use the
         * WebShare API.
         * See: https://developer.mozilla.org/en-US/docs/Web/API/Navigator/share
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link SharePrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse} which
         *         includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onSharePrompt(@NonNull final GeckoSession session,
                                                                    @NonNull final SharePrompt prompt) {
            return null;
        }

        /**
         * Handle a login storage prompt.
         * This is triggered by the user entering new or modified login
         * credentials into a login form.
         *
         * @param session GeckoSession that triggered the prompt.
         * @param prompt The {@link LoginStoragePrompt} that describes the prompt.
         *
         * @return A {@link GeckoResult} resolving to a {@link PromptResponse}
         *         which includes all necessary information to resolve the prompt.
         */
        @UiThread
        default @Nullable GeckoResult<PromptResponse> onLoginStoragePrompt(
                @NonNull final GeckoSession session,
                @NonNull final LoginStoragePrompt prompt) {
            return null;
        }
    }

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
         * Permission for using the WebXR API.
         * See: https://www.w3.org/TR/webxr
         */
        int PERMISSION_XR = 3;

        /**
         * Permission for allowing autoplay of inaudible (silent) video.
         */
        int PERMISSION_AUTOPLAY_INAUDIBLE = 4;

        /**
         * Permission for allowing autoplay of audible video.
         */
        int PERMISSION_AUTOPLAY_AUDIBLE = 5;

        /**
         * Permission for accessing system media keys used to decode DRM media.
         */
        int PERMISSION_MEDIA_KEY_SYSTEM_ACCESS = 6;

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
         *             PERMISSION_XR
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
            PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION,
            PermissionDelegate.PERMISSION_PERSISTENT_STORAGE,
            PermissionDelegate.PERMISSION_XR,
            PermissionDelegate.PERMISSION_AUTOPLAY_INAUDIBLE,
            PermissionDelegate.PERMISSION_AUTOPLAY_AUDIBLE,
            PermissionDelegate.PERMISSION_MEDIA_KEY_SYSTEM_ACCESS})
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
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({TextInputDelegate.RESTART_REASON_FOCUS, TextInputDelegate.RESTART_REASON_BLUR,
            TextInputDelegate.RESTART_REASON_CONTENT_CHANGE})
            /* package */ @interface RestartReason {}

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

    /* package */ void setDynamicToolbarMaxHeight(final int height) {
        if (mDynamicToolbarMaxHeight == height) {
            return;
        }

        if (mHeight != 0 && height != 0 && mHeight < height) {
            Log.w(LOGTAG, new AssertionError(
                    "The maximum height of the dynamic toolbar (" +
                    height +
                    ") should be smaller than GeckoView height (" +
                    mHeight + ")"));
        }

        mDynamicToolbarMaxHeight = height;

        if (mAttachedCompositor) {
            mCompositor.setDynamicToolbarMaxHeight(mDynamicToolbarMaxHeight);
        }
    }


    /* package */ void setFixedBottomOffset(final int offset) {
        if (mFixedBottomOffset == offset) {
            return;
        }

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
        mCompositor.setDynamicToolbarMaxHeight(mDynamicToolbarMaxHeight);
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
                ThreadUtils.postToUiThread(this::onCompositorReady);
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

        if (mFixedBottomOffset != 0) {
            mCompositor.setFixedBottomOffset(mFixedBottomOffset);
        }
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
    }

    /* protected */ void onWindowBoundsChanged() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mHeight != 0 && mDynamicToolbarMaxHeight != 0 &&
            mHeight < mDynamicToolbarMaxHeight) {
            Log.w(LOGTAG, new AssertionError( "The maximum height of the dynamic toolbar (" +
                          mDynamicToolbarMaxHeight +
                          ") should be smaller than GeckoView height (" +
                          mHeight + ")"));
        }

        final int toolbarHeight;
        if (mToolbar != null) {
            toolbarHeight = mToolbar.getCurrentToolbarHeight();
        } else {
            toolbarHeight = 0;
        }

        mClientTop = mTop + toolbarHeight;
        // If the view is not tall enough to even fix the toolbar we just
        // default the client height to 0
        mClientHeight = Math.max(mHeight - toolbarHeight, 0);

        if (mAttachedCompositor) {
            mCompositor.onBoundsChanged(mLeft, mClientTop, mWidth, mClientHeight);
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(mWidth, mClientHeight);
        }
    }

    /* pacakge */ void onSafeAreaInsetsChanged(final int top, final int right, final int bottom, final int left) {
        ThreadUtils.assertOnUiThread();

        if (mAttachedCompositor) {
            mCompositor.onSafeAreaInsetsChanged(top, right, bottom, left);
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


    private Autofill.Support getAutofillSupport() {
        return mAutofillSupport;
    }

    /**
     * Sets the autofill delegate for this session.
     *
     * @param delegate An instance of {@link Autofill.Delegate}.
     */
    @UiThread
    public void setAutofillDelegate(final @Nullable Autofill.Delegate delegate) {
        getAutofillSupport().setDelegate(delegate);
    }

    /**
     * @return The current {@link Autofill.Delegate} for this session, if any.
     */
    @UiThread
    public @Nullable Autofill.Delegate getAutofillDelegate() {
        return getAutofillSupport().getDelegate();
    }

    /**
     * Perform autofill using the specified values.
     *
     * @param values Map of autofill IDs to values.
     */
    @UiThread
    public void autofill(final @NonNull SparseArray<CharSequence> values) {
        getAutofillSupport().autofill(values);
    }

    /**
     * Provides an autofill structure similar to {@link View#onProvideAutofillVirtualStructure(ViewStructure, int)} , but
     * does not rely on {@link ViewStructure} to build the tree. This is useful for apps that want
     * to provide autofill functionality without using the Android autofill system or requiring
     * API 26.
     *
     * @return The elements available for autofill.
     */
    @UiThread
    public @NonNull Autofill.Session getAutofillSession() {
        return getAutofillSupport().getAutofillSession();
    }

    private static String rgbaToArgb(final String color) {
        // We expect #rrggbbaa
        if (color.length() != 9 || !color.startsWith("#")) {
            throw new IllegalArgumentException("Invalid color format");
        }

        return "#" + color.substring(7) + color.substring(1, 7);
    }

    private static void fixupManifestColor(final JSONObject manifest, final String name) throws JSONException {
        if (manifest.isNull(name)) {
            return;
        }

        manifest.put(name, rgbaToArgb(manifest.getString(name)));
    }

    private static JSONObject fixupWebAppManifest(final JSONObject manifest) {
        // Colors are #rrggbbaa, but we want them to be #aarrggbb, since that's what
        // android.graphics.Color expects.
        try {
            fixupManifestColor(manifest, "theme_color");
            fixupManifestColor(manifest, "background_color");
        } catch (JSONException e) {
            Log.w(LOGTAG, "Failed to fixup web app manifest", e);
        }

        return manifest;
    }
}
