/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Arrays;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.LayerSession;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.net.Uri;
import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;

public class GeckoSession extends LayerSession
                          implements Parcelable {
    private static final String LOGTAG = "GeckoSession";
    private static final boolean DEBUG = false;

    /* package */ enum State implements NativeQueue.State {
        INITIAL(0),
        READY(1);

        private final int mRank;

        private State(int rank) {
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

    private final TextInputController mTextInput = new TextInputController(this, mNativeQueue);

    private final GeckoSessionHandler<ContentListener> mContentHandler =
        new GeckoSessionHandler<ContentListener>(
            "GeckoViewContent", this,
            new String[]{
                "GeckoView:ContextMenu",
                "GeckoView:DOMTitleChanged",
                "GeckoView:FullScreenEnter",
                "GeckoView:FullScreenExit"
            }
        ) {
            @Override
            public void handleMessage(final ContentListener listener,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if ("GeckoView:ContextMenu".equals(event)) {
                    listener.onContextMenu(GeckoSession.this,
                                           message.getInt("screenX"),
                                           message.getInt("screenY"),
                                           message.getString("uri"),
                                           message.getString("elementSrc"));
                } else if ("GeckoView:DOMTitleChanged".equals(event)) {
                    listener.onTitleChange(GeckoSession.this,
                                           message.getString("title"));
                } else if ("GeckoView:FullScreenEnter".equals(event)) {
                    listener.onFullScreen(GeckoSession.this, true);
                } else if ("GeckoView:FullScreenExit".equals(event)) {
                    listener.onFullScreen(GeckoSession.this, false);
                }
            }
        };

    private final GeckoSessionHandler<NavigationListener> mNavigationHandler =
        new GeckoSessionHandler<NavigationListener>(
            "GeckoViewNavigation", this,
            new String[]{
                "GeckoView:LocationChange",
                "GeckoView:OnLoadUri"
            }
        ) {
            @Override
            public void handleMessage(final NavigationListener listener,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                if ("GeckoView:LocationChange".equals(event)) {
                    listener.onLocationChange(GeckoSession.this,
                                              message.getString("uri"));
                    listener.onCanGoBack(GeckoSession.this,
                                         message.getBoolean("canGoBack"));
                    listener.onCanGoForward(GeckoSession.this,
                                            message.getBoolean("canGoForward"));
                } else if ("GeckoView:OnLoadUri".equals(event)) {
                    final String uri = message.getString("uri");
                    final NavigationListener.TargetWindow where =
                        NavigationListener.TargetWindow.forGeckoValue(
                            message.getInt("where"));
                    final boolean result =
                        listener.onLoadUri(GeckoSession.this, uri, where);
                    callback.sendSuccess(result);
                }
            }
        };

    private final GeckoSessionHandler<ProgressListener> mProgressHandler =
        new GeckoSessionHandler<ProgressListener>(
            "GeckoViewProgress", this,
            new String[]{
                "GeckoView:PageStart",
                "GeckoView:PageStop",
                "GeckoView:SecurityChanged"
            }
        ) {
            @Override
            public void handleMessage(final ProgressListener listener,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {
                if ("GeckoView:PageStart".equals(event)) {
                    listener.onPageStart(GeckoSession.this,
                                         message.getString("uri"));
                } else if ("GeckoView:PageStop".equals(event)) {
                    listener.onPageStop(GeckoSession.this,
                                        message.getBoolean("success"));
                } else if ("GeckoView:SecurityChanged".equals(event)) {
                    final GeckoBundle identity = message.getBundle("identity");
                    listener.onSecurityChange(GeckoSession.this, new ProgressListener.SecurityInformation(identity));
                }
            }
        };

    private final GeckoSessionHandler<ScrollListener> mScrollHandler =
        new GeckoSessionHandler<ScrollListener>(
            "GeckoViewScroll", this,
            new String[]{ "GeckoView:ScrollChanged" }
        ) {
            @Override
            public void handleMessage(final ScrollListener listener,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if ("GeckoView:ScrollChanged".equals(event)) {
                    listener.onScrollChanged(GeckoSession.this,
                                             message.getInt("scrollX"),
                                             message.getInt("scrollY"));
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
            }, /* alwaysListen */ true
        ) {
            @Override
            public void handleMessage(final PermissionDelegate listener,
                                      final String event,
                                      final GeckoBundle message,
                                      final EventCallback callback) {

                if (listener == null) {
                    callback.sendSuccess(/* granted */ false);
                    return;
                }
                if ("GeckoView:AndroidPermission".equals(event)) {
                    listener.requestAndroidPermissions(
                            GeckoSession.this, message.getStringArray("perms"),
                            new PermissionCallback("android", callback));
                } else if ("GeckoView:ContentPermission".equals(event)) {
                    final String type = message.getString("perm");
                    listener.requestContentPermission(
                            GeckoSession.this, message.getString("uri"),
                            type, message.getString("access"),
                            new PermissionCallback(type, callback));
                } else if ("GeckoView:MediaPermission".equals(event)) {
                    listener.requestMediaPermission(
                            GeckoSession.this, message.getString("uri"),
                            message.getBundleArray("video"), message.getBundleArray("audio"),
                            new PermissionCallback("media", callback));
                }
            }
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
        public void grant(final GeckoBundle video, final GeckoBundle audio) {
            grant(video != null ? video.getString("id") : null,
                  audio != null ? audio.getString("id") : null);
        }
    }

    /**
     * Get the current prompt delegate for this GeckoSession.
     * @return PromptDelegate instance or null if using default delegate.
     */
    public PermissionDelegate getPermissionDelegate() {
        return mPermissionHandler.getListener();
    }

    /**
     * Set the current permission delegate for this GeckoSession.
     * @param delegate PermissionDelegate instance or null to use the default delegate.
     */
    public void setPermissionDelegate(final PermissionDelegate delegate) {
        mPermissionHandler.setListener(delegate, this);
    }

    private PromptDelegate mPromptDelegate;

    private final Listener mListener = new Listener();

    protected static final class Window extends JNIObject implements IInterface {
        private NativeQueue mNativeQueue;
        private Binder mBinder;

        public Window(final NativeQueue nativeQueue) {
            mNativeQueue = nativeQueue;
        }

        @Override // IInterface
        public IBinder asBinder() {
            if (mBinder == null) {
                mBinder = new Binder();
                mBinder.attachInterface(this, Window.class.getName());
            }
            return mBinder;
        }

        @WrapForJNI(dispatchTo = "proxy")
        public static native void open(Window instance, Compositor compositor,
                                       EventDispatcher dispatcher,
                                       GeckoBundle settings, String chromeUri,
                                       int screenId, boolean privateMode);

        @WrapForJNI(dispatchTo = "proxy")
        @Override protected native void disposeNative();

        @WrapForJNI(dispatchTo = "proxy")
        public native void close();

        @WrapForJNI(dispatchTo = "proxy")
        public native void transfer(Compositor compositor, EventDispatcher dispatcher,
                                    GeckoBundle settings);

        @WrapForJNI(calledFrom = "gecko")
        private synchronized void onTransfer(final EventDispatcher dispatcher) {
            final NativeQueue nativeQueue = dispatcher.getNativeQueue();
            if (mNativeQueue != nativeQueue) {
                // Set new queue to the same state as the old queue,
                // then return the old queue to its initial state if applicable,
                // because the old queue is no longer the active queue.
                nativeQueue.setState(mNativeQueue.getState());
                mNativeQueue.checkAndSetState(State.READY, State.INITIAL);
                mNativeQueue = nativeQueue;
            }
        }

        @WrapForJNI(dispatchTo = "proxy")
        public native void attachEditable(IGeckoEditableParent parent,
                                          GeckoEditableChild child);

        @WrapForJNI(calledFrom = "gecko")
        private synchronized void onReady() {
            if (mNativeQueue.checkAndSetState(State.INITIAL, State.READY)) {
                Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                      " - chrome startup finished");
            }
        }
    }

    private class Listener implements BundleEventListener {
        /* package */ void registerListeners() {
            getEventDispatcher().registerUiThreadListener(this,
                "GeckoView:Prompt",
                null);
        }

        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if (DEBUG) {
                Log.d(LOGTAG, "handleMessage: event = " + event);
            }

            if ("GeckoView:Prompt".equals(event)) {
                handlePromptEvent(GeckoSession.this, message, callback);
            }
        }
    }

    protected Window mWindow;
    private GeckoSessionSettings mSettings;

    public GeckoSession() {
        this(/* settings */ null);
    }

    public GeckoSession(final GeckoSessionSettings settings) {
        if (settings == null) {
            mSettings = new GeckoSessionSettings(this);
        } else {
            mSettings = new GeckoSessionSettings(settings, this);
        }

        mListener.registerListeners();
    }

    private void transferFrom(final Window window, final GeckoSessionSettings settings) {
        if (isOpen()) {
            throw new IllegalStateException("Session is open");
        }

        mWindow = window;
        mSettings = new GeckoSessionSettings(settings, this);

        if (mWindow != null) {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                mWindow.transfer(mCompositor, mEventDispatcher, mSettings.asBundle());
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        mWindow, "transfer",
                        Compositor.class, mCompositor,
                        EventDispatcher.class, mEventDispatcher,
                        GeckoBundle.class, mSettings.asBundle());
            }
        }

        onWindowChanged();
    }

    /* package */ void transferFrom(final GeckoSession session) {
        transferFrom(session.mWindow, session.mSettings);
        session.mWindow = null;
        session.onWindowChanged();
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        out.writeStrongInterface(mWindow);
        out.writeParcelable(mSettings, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        final IBinder binder = source.readStrongBinder();
        final IInterface ifce = (binder != null) ?
                binder.queryLocalInterface(Window.class.getName()) : null;
        final Window window = (ifce instanceof Window) ? (Window) ifce : null;
        final GeckoSessionSettings settings =
                source.readParcelable(getClass().getClassLoader());
        transferFrom(window, settings);
    }

    public static final Creator<GeckoSession> CREATOR = new Creator<GeckoSession>() {
        @Override
        public GeckoSession createFromParcel(final Parcel in) {
            final GeckoSession session = new GeckoSession();
            session.readFromParcel(in);
            return session;
        }

        @Override
        public GeckoSession[] newArray(final int size) {
            return new GeckoSession[size];
        }
    };

    /**
     * Preload GeckoSession by starting Gecko in the background, if Gecko is not already running.
     *
     * @param context Activity or Application Context for starting GeckoSession.
     */
    public static void preload(final Context context) {
        preload(context, /* geckoArgs */ null, /* multiprocess */ false);
    }

    /**
     * Preload GeckoSession by starting Gecko with the specified arguments in the background,
     * if Gecko is not already running.
     *
     * @param context Activity or Application Context for starting GeckoSession.
     * @param geckoArgs Arguments to be passed to Gecko, if Gecko is not already running.
     * @param multiprocess True if child process in multiprocess mode should be preloaded.
     */
    public static void preload(final Context context, final String geckoArgs,
                               final boolean multiprocess) {
        final Context appContext = context.getApplicationContext();
        if (GeckoAppShell.getApplicationContext() == null) {
            GeckoAppShell.setApplicationContext(appContext);
        }

        final int flags = multiprocess ? GeckoThread.FLAG_PRELOAD_CHILD : 0;
        if (GeckoThread.initMainProcess(/* profile */ null, geckoArgs, flags)) {
            GeckoThread.launch();
        }
    }

    public boolean isOpen() {
        return mWindow != null;
    }

    public void openWindow(final Context appContext) {
        ThreadUtils.assertOnUiThread();

        if (isOpen()) {
            throw new IllegalStateException("Session is open");
        }

        if (!GeckoThread.isLaunched()) {
            final boolean multiprocess =
                    mSettings.getBoolean(GeckoSessionSettings.USE_MULTIPROCESS);
            preload(appContext, /* geckoArgs */ null, multiprocess);
        }

        final String chromeUri = mSettings.getString(GeckoSessionSettings.CHROME_URI);
        final int screenId = mSettings.getInt(GeckoSessionSettings.SCREEN_ID);
        final boolean isPrivate = mSettings.getBoolean(GeckoSessionSettings.USE_PRIVATE_MODE);

        mWindow = new Window(mNativeQueue);

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            Window.open(mWindow, mCompositor, mEventDispatcher,
                        mSettings.asBundle(), chromeUri, screenId, isPrivate);
        } else {
            GeckoThread.queueNativeCallUntil(
                GeckoThread.State.PROFILE_READY,
                Window.class, "open",
                Window.class, mWindow,
                Compositor.class, mCompositor,
                EventDispatcher.class, mEventDispatcher,
                GeckoBundle.class, mSettings.asBundle(),
                String.class, chromeUri,
                screenId, isPrivate);
        }

        onWindowChanged();
    }

    public void closeWindow() {
        ThreadUtils.assertOnUiThread();

        if (!isOpen()) {
            throw new IllegalStateException("Session is not open");
        }

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            mWindow.close();
            mWindow.disposeNative();
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    mWindow, "close");
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    mWindow, "disposeNative");
        }

        mWindow = null;
        onWindowChanged();
    }

    private void onWindowChanged() {
        if (mWindow != null) {
            mTextInput.onWindowChanged(mWindow);
        }
    }

    /**
     * Get the TextInputController instance for this session.
     *
     * @return TextInputController instance.
     */
    public @NonNull TextInputController getTextInputController() {
        // May be called on any thread.
        return mTextInput;
    }

    /**
    * Load the given URI.
    * @param uri The URI of the resource to load.
    */
    public void loadUri(String uri) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putString("uri", uri);
        mEventDispatcher.dispatch("GeckoView:LoadUri", msg);
    }

    /**
    * Load the given URI.
    * @param uri The URI of the resource to load.
    */
    public void loadUri(Uri uri) {
        loadUri(uri.toString());
    }

    /**
    * Reload the current URI.
    */
    public void reload() {
        mEventDispatcher.dispatch("GeckoView:Reload", null);
    }

    /**
    * Stop loading.
    */
    public void stop() {
        mEventDispatcher.dispatch("GeckoView:Stop", null);
    }

    /**
    * Go back in history.
    */
    public void goBack() {
        mEventDispatcher.dispatch("GeckoView:GoBack", null);
    }

    /**
    * Go forward in history.
    */
    public void goForward() {
        mEventDispatcher.dispatch("GeckoView:GoForward", null);
    }

    /**
    * Set this GeckoSession as active or inactive. Setting a GeckoSession to inactive will
    * significantly reduce its memory footprint, but should only be done if the
    * GeckoSession is not currently visible.
    * @param active A boolean determining whether the GeckoSession is active
    */
    public void setActive(boolean active) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putBoolean("active", active);
        mEventDispatcher.dispatch("GeckoView:SetActive", msg);
    }

    public GeckoSessionSettings getSettings() {
        return mSettings;
    }

    public void importScript(final String url) {
        if (url.startsWith("resource://android/assets/")) {
            final GeckoBundle data = new GeckoBundle(1);
            data.putString("scriptURL", url);
            getEventDispatcher().dispatch("GeckoView:ImportScript", data);
            return;
        }

        throw new IllegalArgumentException("Must import script from 'resources://android/assets/' location.");
    }

    /**
    * Exits fullscreen mode
    */
    public void exitFullScreen() {
        mEventDispatcher.dispatch("GeckoViewContent:ExitFullScreen", null);
    }

    /**
    * Set the content callback handler.
    * This will replace the current handler.
    * @param listener An implementation of ContentListener.
    */
    public void setContentListener(ContentListener listener) {
        mContentHandler.setListener(listener, this);
    }

    /**
    * Get the content callback handler.
    * @return The current content callback handler.
    */
    public ContentListener getContentListener() {
        return mContentHandler.getListener();
    }

    /**
    * Set the progress callback handler.
    * This will replace the current handler.
    * @param listener An implementation of ProgressListener.
    */
    public void setProgressListener(ProgressListener listener) {
        mProgressHandler.setListener(listener, this);
    }

    /**
    * Get the progress callback handler.
    * @return The current progress callback handler.
    */
    public ProgressListener getProgressListener() {
        return mProgressHandler.getListener();
    }

    /**
    * Set the navigation callback handler.
    * This will replace the current handler.
    * @param listener An implementation of NavigationListener.
    */
    public void setNavigationListener(NavigationListener listener) {
        mNavigationHandler.setListener(listener, this);
    }

    /**
    * Get the navigation callback handler.
    * @return The current navigation callback handler.
    */
    public NavigationListener getNavigationListener() {
        return mNavigationHandler.getListener();
    }

    /**
    * Set the content scroll callback handler.
    * This will replace the current handler.
    * @param listener An implementation of ScrollListener.
    */
    public void setScrollListener(ScrollListener listener) {
        mScrollHandler.setListener(listener, this);
    }

    /**
     * Set the current prompt delegate for this GeckoSession.
     * @param delegate PromptDelegate instance or null to use the built-in delegate.
     */
    public void setPromptDelegate(PromptDelegate delegate) {
        mPromptDelegate = delegate;
    }

    /**
     * Get the current prompt delegate for this GeckoSession.
     * @return PromptDelegate instance or null if using built-in delegate.
     */
    public PromptDelegate getPromptDelegate() {
        return mPromptDelegate;
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
        public void confirm(GeckoBundle item) {
            if ("choice".equals(mType)) {
                confirm(item == null ? null : item.getString("id"));
                return;
            } else {
                throw new UnsupportedOperationException();
            }
        }

        @Override // ChoiceCallback
        public void confirm(GeckoBundle[] items) {
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
                    ids[i] = (items[i] == null) ? null : items[i].getString("id");
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
                delegate.alert(session, title, msg, cb);
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
                delegate.promptForButton(session, title, msg, btnCustomTitle, cb);
                break;
            }
            case "text": {
                delegate.promptForText(session, title, msg, message.getString("value"), cb);
                break;
            }
            case "auth": {
                delegate.promptForAuth(session, title, msg, message.getBundle("options"), cb);
                break;
            }
            case "choice": {
                final int intMode;
                if ("menu".equals(mode)) {
                    intMode = PromptDelegate.CHOICE_TYPE_MENU;
                } else if ("single".equals(mode)) {
                    intMode = PromptDelegate.CHOICE_TYPE_SINGLE;
                } else if ("multiple".equals(mode)) {
                    intMode = PromptDelegate.CHOICE_TYPE_MULTIPLE;
                } else {
                    callback.sendError("Invalid mode");
                    return;
                }
                delegate.promptForChoice(session, title, msg, intMode,
                                         message.getBundleArray("choices"), cb);
                break;
            }
            case "color": {
                delegate.promptForColor(session, title, message.getString("value"), cb);
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
                delegate.promptForDateTime(session, title, intMode,
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
                final String[] extensions = message.getStringArray("extension");
                if (extensions != null) {
                    final ArrayList<String> combined =
                            new ArrayList<>(mimeTypes.length + extensions.length);
                    combined.addAll(Arrays.asList(mimeTypes));
                    for (final String extension : extensions) {
                        final String mimeType =
                                URLConnection.guessContentTypeFromName(extension);
                        if (mimeType != null) {
                            combined.add(mimeType);
                        }
                    }
                    mimeTypes = combined.toArray(new String[combined.size()]);
                }
                delegate.promptForFile(session, title, intMode, mimeTypes, cb);
                break;
            }
            default: {
                callback.sendError("Invalid type");
                break;
            }
        }
    }

    public EventDispatcher getEventDispatcher() {
        return mEventDispatcher;
    }

    public interface ProgressListener {
        /**
         * Class representing security information for a site.
         */
        public class SecurityInformation {
            public static final int SECURITY_MODE_UNKNOWN = 0;
            public static final int SECURITY_MODE_IDENTIFIED = 1;
            public static final int SECURITY_MODE_VERIFIED = 2;

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
            public final String origin;
            /**
             * Contains the host associated with the certificate.
             */
            public final String host;
            /**
             * Contains the human-readable name of the certificate subject.
             */
            public final String organization;
            /**
             * Contains the full name of the certificate subject, including location.
             */
            public final String subjectName;
            /**
             * Contains the common name of the issuing authority.
             */
            public final String issuerCommonName;
            /**
             * Contains the full/proper name of the issuing authority.
             */
            public final String issuerOrganization;
            /**
             * Indicates the security level of the site; possible values are SECURITY_MODE_UNKNOWN,
             * SECURITY_MODE_IDENTIFIED, and SECURITY_MODE_VERIFIED. SECURITY_MODE_IDENTIFIED
             * indicates domain validation only, while SECURITY_MODE_VERIFIED indicates extended validation.
             */
            public final int securityMode;
            /**
             * Indicates the presence of passive mixed content; possible values are
             * CONTENT_UNKNOWN, CONTENT_BLOCKED, and CONTENT_LOADED.
             */
            public final int mixedModePassive;
            /**
             * Indicates the presence of active mixed content; possible values are
             * CONTENT_UNKNOWN, CONTENT_BLOCKED, and CONTENT_LOADED.
             */
            public final int mixedModeActive;
            /**
             * Indicates the status of tracking protection; possible values are
             * CONTENT_UNKNOWN, CONTENT_BLOCKED, and CONTENT_LOADED.
             */
            public final int trackingMode;

            /* package */ SecurityInformation(GeckoBundle identityData) {
                final GeckoBundle mode = identityData.getBundle("mode");

                mixedModePassive = mode.getInt("mixed_display");
                mixedModeActive = mode.getInt("mixed_active");
                trackingMode = mode.getInt("tracking");

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
        }

        /**
        * A View has started loading content from the network.
        * @param view The GeckoSession that initiated the callback.
        * @param url The resource being loaded.
        */
        void onPageStart(GeckoSession session, String url);

        /**
        * A View has finished loading content from the network.
        * @param view The GeckoSession that initiated the callback.
        * @param success Whether the page loaded successfully or an error occurred.
        */
        void onPageStop(GeckoSession session, boolean success);

        /**
        * The security status has been updated.
        * @param view The GeckoSession that initiated the callback.
        * @param securityInfo The new security information.
        */
        void onSecurityChange(GeckoSession session, SecurityInformation securityInfo);
    }

    public interface ContentListener {
        /**
        * A page title was discovered in the content or updated after the content
        * loaded.
        * @param view The GeckoSession that initiated the callback.
        * @param title The title sent from the content.
        */
        void onTitleChange(GeckoSession session, String title);

        /**
         * A page has entered or exited full screen mode. Typically, the implementation
         * would set the Activity containing the GeckoSession to full screen when the page is
         * in full screen mode.
         *
         * @param view The GeckoSession that initiated the callback.
         * @param fullScreen True if the page is in full screen mode.
         */
        void onFullScreen(GeckoSession session, boolean fullScreen);


        /**
         * A user has initiated the context menu via long-press.
         * This event is fired on links, (nested) images and (nested) media
         * elements.
         *
         * @param view The GeckoSession that initiated the callback.
         * @param screenX The screen coordinates of the press.
         * @param screenY The screen coordinates of the press.
         * @param uri The URI of the pressed link, set for links and
         *            image-links.
         * @param elementSrc The source URI of the pressed element, set for
         *                   (nested) images and media elements.
         */
        void onContextMenu(GeckoSession session, int screenX, int screenY,
                           String uri, String elementSrc);
    }

    public interface NavigationListener {
        /**
        * A view has started loading content from the network.
        * @param view The GeckoSession that initiated the callback.
        * @param url The resource being loaded.
        */
        void onLocationChange(GeckoSession session, String url);

        /**
        * The view's ability to go back has changed.
        * @param view The GeckoSession that initiated the callback.
        * @param canGoBack The new value for the ability.
        */
        void onCanGoBack(GeckoSession session, boolean canGoBack);

        /**
        * The view's ability to go forward has changed.
        * @param view The GeckoSession that initiated the callback.
        * @param canGoForward The new value for the ability.
        */
        void onCanGoForward(GeckoSession session, boolean canGoForward);

        enum TargetWindow {
            DEFAULT(0),
            CURRENT(1),
            NEW(2);

            private static final TargetWindow[] sValues = TargetWindow.values();
            private int mValue;

            private TargetWindow(int value) {
                mValue = value;
            }

            public static TargetWindow forValue(int value) {
                return sValues[value];
            }

            public static TargetWindow forGeckoValue(int value) {
                // DEFAULT(0),
                // CURRENT(1),
                // NEW(2),
                // NEWTAB(3),
                // SWITCHTAB(4);
                final TargetWindow[] sMap = {
                    DEFAULT,
                    CURRENT,
                    NEW,
                    NEW,
                    NEW
                };
                return sMap[value];
            }
        }

        enum LoadUriResult {
            HANDLED(0),
            LOAD_IN_FRAME(1);

            private int mValue;

            private LoadUriResult(int value) {
                mValue = value;
            }
        }

        /**
        * A request to open an URI.
        * @param view The GeckoSession that initiated the callback.
        * @param uri The URI to be loaded.
        * @param where The target window.
        *
        * @return True if the URI loading has been handled, false if Gecko
        *         should handle the loading.
        */
        boolean onLoadUri(GeckoSession session, String uri, TargetWindow where);
    }

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
            void dismiss();

            /**
             * Return whether the prompt shown should include a checkbox. For example, if
             * a page shows multiple prompts within a short period of time, the next
             * prompt will include a checkbox to let the user disable future prompts.
             * Although the API allows checkboxes for all prompts, in practice, only
             * alert/button/text/auth prompts will possibly have a checkbox.
             */
            boolean hasCheckbox();

            /**
             * Return the message label for the optional checkbox.
             */
            String getCheckboxMessage();

            /**
             * Return the initial value for the optional checkbox.
             */
            boolean getCheckboxValue();

            /**
             * Set the current value for the optional checkbox.
             */
            void setCheckboxValue(boolean value);
        }

        /**
         * Display a simple message prompt.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param callback Callback interface.
         */
        void alert(GeckoSession session, String title, String msg, AlertCallback callback);

        /**
         * Callback interface for notifying the result of a button prompt.
         */
        interface ButtonCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the button prompt is dismissed by
             * the user pressing one of the buttons.
             */
            void confirm(int button);
        }

        static final int BUTTON_TYPE_POSITIVE = 0;
        static final int BUTTON_TYPE_NEUTRAL = 1;
        static final int BUTTON_TYPE_NEGATIVE = 2;

        /**
         * Display a prompt with up to three buttons.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param btnMsg Array of 3 elements indicating labels for the individual buttons.
         *               btnMsg[BUTTON_TYPE_POSITIVE] is the label for the "positive" button.
         *               btnMsg[BUTTON_TYPE_NEUTRAL] is the label for the "neutral" button.
         *               btnMsg[BUTTON_TYPE_NEGATIVE] is the label for the "negative" button.
         *               The button is hidden if the corresponding label is null.
         * @param callback Callback interface.
         */
        void promptForButton(GeckoSession session, String title, String msg,
                             String[] btnMsg, ButtonCallback callback);

        /**
         * Callback interface for notifying the result of prompts that have text results,
         * including color and date/time pickers.
         */
        interface TextCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when the text prompt is confirmed by
             * the user, for example by pressing the "OK" button.
             */
            void confirm(String text);
        }

        /**
         * Display a prompt for inputting text.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param value Default input text for the prompt.
         * @param callback Callback interface.
         */
        void promptForText(GeckoSession session, String title, String msg,
                           String value, TextCallback callback);

        /**
         * Callback interface for notifying the result of authentication prompts.
         */
        interface AuthCallback extends AlertCallback {
            /**
             * Called by the prompt implementation when a password-only prompt is
             * confirmed by the user.
             */
            void confirm(String password);

            /**
             * Called by the prompt implementation when a username/password prompt is
             * confirmed by the user.
             */
            void confirm(String username, String password);
        }

        /**
         * The auth prompt is for a network host.
         */
        static final int AUTH_FLAG_HOST = 1;
        /**
         * The auth prompt is for a proxy.
         */
        static final int AUTH_FLAG_PROXY = 2;
        /**
         * The auth prompt should only request a password.
         */
        static final int AUTH_FLAG_ONLY_PASSWORD = 8;
        /**
         * The auth prompt is the result of a previous failed login.
         */
        static final int AUTH_FLAG_PREVIOUS_FAILED = 16;
        /**
         * The auth prompt is for a cross-origin sub-resource.
         */
        static final int AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE = 32;

        /**
         * The auth request is unencrypted or the encryption status is unknown.
         */
        static final int AUTH_LEVEL_NONE = 0;
        /**
         * The auth request only encrypts password but not data.
         */
        static final int AUTH_LEVEL_PW_ENCRYPTED = 1;
        /**
         * The auth request encrypts both password and data.
         */
        static final int AUTH_LEVEL_SECURE = 2;

        /**
         * Display a prompt for authentication credentials.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param msg Message for the prompt dialog.
         * @param options Bundle containing options for the prompt with keys,
         *                "flags": int, bit field of AUTH_FLAG_* flags;
         *                "uri": String, URI for the auth request or null if unknown;
         *                "level": int, one of AUTH_LEVEL_* indicating level of encryption;
         *                "username": String, initial username or null if password-only;
         *                "password": String, intiial password;
         * @param callback Callback interface.
         */
        void promptForAuth(GeckoSession session, String title, String msg,
                           GeckoBundle options, AuthCallback callback);

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
            void confirm(String id);

            /**
             * Called by the prompt implementation when the multiple-choice list is
             * dismissed by the user.
             *
             * @param ids IDs of the selected items.
             */
            void confirm(String[] ids);

            /**
             * Called by the prompt implementation when the menu or single-choice list is
             * dismissed by the user.
             *
             * @param item Bundle representing the selected item; must be an original
             *             GeckoBundle object that was passed to the implementation.
             */
            void confirm(GeckoBundle item);

            /**
             * Called by the prompt implementation when the multiple-choice list is
             * dismissed by the user.
             *
             * @param items Bundle array representing the selected items; must be original
             *             GeckoBundle objects that were passed to the implementation.
             */
            void confirm(GeckoBundle[] items);
        }

        /**
         * Display choices in a menu that dismisses as soon as an item is chosen.
         */
        static final int CHOICE_TYPE_MENU = 1;

        /**
         * Display choices in a list that allows a single selection.
         */
        static final int CHOICE_TYPE_SINGLE = 2;

        /**
         * Display choices in a list that allows multiple selections.
         */
        static final int CHOICE_TYPE_MULTIPLE = 3;

        /**
         * Display a menu prompt or list prompt.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog, or null for no title.
         * @param msg Message for the prompt dialog, or null for no message.
         * @param type One of CHOICE_TYPE_* indicating the type of prompt.
         * @param choices Array of bundles each representing an item or group, with keys,
         *                "disabled": boolean, true if the item should not be selectable;
         *                "icon": String, URI of the item icon or null if none
         *                        (only valid for menus);
         *                "id": String, ID of the item or group;
         *                "items": GeckoBundle[], array of sub-items in a group or null
         *                         if not a group.
         *                "label": String, label for displaying the item or group;
         *                "selected": boolean, true if the item should be pre-selected
         *                            (pre-checked for menu items);
         *                "separator": boolean, true if the item should be a menu separator
         *                             (only valid for menus);
         * @param callback Callback interface.
         */
        void promptForChoice(GeckoSession session, String title, String msg, int type,
                             GeckoBundle[] choices, ChoiceCallback callback);

        /**
         * Display a color prompt.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param value Initial color value in HTML color format.
         * @param callback Callback interface; the result passed to confirm() must be in
         *                 HTML color format.
         */
        void promptForColor(GeckoSession session, String title, String value,
                            TextCallback callback);

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
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog; currently always null.
         * @param type One of DATETIME_TYPE_* indicating the type of prompt.
         * @param value Initial date/time value in HTML date/time format.
         * @param min Minimum date/time value in HTML date/time format.
         * @param max Maximum date/time value in HTML date/time format.
         * @param callback Callback interface; the result passed to confirm() must be in
         *                 HTML date/time format.
         */
        void promptForDateTime(GeckoSession session, String title, int type,
                               String value, String min, String max, TextCallback callback);

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
            void confirm(Context context, Uri uri);

            /**
             * Called by the prompt implementation when the user makes file selections in
             * multiple-selection mode.
             *
             * @param context An application Context for parsing URIs.
             * @param uris Array of URI objects for the selected files.
             */
            void confirm(Context context, Uri[] uris);
        }

        static final int FILE_TYPE_SINGLE = 1;
        static final int FILE_TYPE_MULTIPLE = 2;

        /**
         * Display a file prompt.
         *
         * @param view The GeckoSession that triggered the prompt
         *             or null if the prompt is a global prompt.
         * @param title Title for the prompt dialog.
         * @param type One of FILE_TYPE_* indicating the prompt type.
         * @param mimeTypes Array of permissible MIME types for the selected files, in
         *                  the form "type/subtype", where "type" and/or "subtype" can be
         *                  "*" to indicate any value.
         * @param callback Callback interface.
         */
        void promptForFile(GeckoSession session, String title, int type,
                           String[] mimeTypes, FileCallback callback);
    }

    /**
     * GeckoSession applications implement this interface to handle content scroll
     * events.
     **/
    public interface ScrollListener {
        /**
         * The scroll position of the content has changed.
         *
        * @param view The GeckoSession that initiated the callback.
        * @param scrollX The new horizontal scroll position in pixels.
        * @param scrollY The new vertical scroll position in pixels.
        */
        public void onScrollChanged(GeckoSession session, int scrollX, int scrollY);
    }

    /**
     * GeckoSession applications implement this interface to handle requests for permissions
     * from content, such as geolocation and notifications. For each permission, usually
     * two requests are generated: one request for the Android app permission through
     * requestAppPermissions, which is typically handled by a system permission dialog;
     * and another request for the content permission (e.g. through
     * requestContentPermission), which is typically handled by an app-specific
     * permission dialog.
     **/
    public interface PermissionDelegate {
        /**
         * Callback interface for notifying the result of a permission request.
         */
        interface Callback {
            /**
             * Called by the implementation after permissions are granted; the
             * implementation must call either grant() or reject() for every request.
             */
            void grant();

            /**
             * Called by the implementation when permissions are not granted; the
             * implementation must call either grant() or reject() for every request.
             */
            void reject();
        }

        /**
         * Request Android app permissions.
         *
         * @param view GeckoSession instance requesting the permissions.
         * @param permissions List of permissions to request; possible values are,
         *                    android.Manifest.permission.ACCESS_FINE_LOCATION
         *                    android.Manifest.permission.CAMERA
         *                    android.Manifest.permission.RECORD_AUDIO
         * @param callback Callback interface.
         */
        void requestAndroidPermissions(GeckoSession session, String[] permissions,
                                       Callback callback);

        /**
         * Request content permission.
         *
         * @param view GeckoSession instance requesting the permission.
         * @param uri The URI of the content requesting the permission.
         * @param type The type of the requested permission; possible values are,
         *             "geolocation": permission for using the geolocation API
         *             "desktop-notification": permission for using the notifications API
         * @param access Not used.
         * @param callback Callback interface.
         */
        void requestContentPermission(GeckoSession session, String uri, String type,
                                      String access, Callback callback);

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
            void grant(final String video, final String audio);

            /**
             * Called by the implementation after permissions are granted; the
             * implementation must call one of grant() or reject() for every request.
             *
             * @param video Bundle for the video source to use (must be an original
             *              GeckoBundle object that was passed to the implementation);
             *              or null when video is not requested.
             * @param audio Bundle for the audio source to use (must be an original
             *              GeckoBundle object that was passed to the implementation);
             *              or null when audio is not requested.
             */
            void grant(final GeckoBundle video, final GeckoBundle audio);

            /**
             * Called by the implementation when permissions are not granted; the
             * implementation must call one of grant() or reject() for every request.
             */
            void reject();
        }

        /**
         * Request content media permissions, including request for which video and/or
         * audio source to use.
         *
         * @param view GeckoSession instance requesting the permission.
         * @param uri The URI of the content requesting the permission.
         * @param video List of video sources, or null if not requesting video.
         *              Each bundle represents a video source, with keys,
         *              "id": String, the origin-specific source identifier;
         *              "rawId": String, the non-origin-specific source identifier;
         *              "name": String, the name of the video source from the system
         *                      (for example, "Camera 0, Facing back, Orientation 90");
         *                      may be empty;
         *              "mediaSource": String, the media source type; possible values are,
         *                             "camera", "screen", "application", "window",
         *                             "browser", and "other";
         *              "type": String, always "video";
         * @param audio List of audio sources, or null if not requesting audio.
         *              Each bundle represents an audio source with same keys and possible
         *              values as video source bundles above, except for:
         *              "mediaSource", String; possible values are "microphone",
         *                             "audioCapture", and "other";
         *              "type", String, always "audio";
         * @param callback Callback interface.
         */
        void requestMediaPermission(GeckoSession session, String uri, GeckoBundle[] video,
                                    GeckoBundle[] audio, MediaCallback callback);
    }
}
