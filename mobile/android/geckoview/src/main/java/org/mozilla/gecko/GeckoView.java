/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Set;

import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.NativeQueue.StateHolder;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public class GeckoView extends LayerView
    implements ContextGetter {

    private static final String DEFAULT_SHARED_PREFERENCES_FILE = "GeckoView";
    private static final String LOGTAG = "GeckoView";

    private static final boolean DEBUG = false;

    /* package */ enum State implements NativeQueue.State {
        @WrapForJNI INITIAL(0),
        @WrapForJNI READY(1);

        private int rank;

        private State(int rank) {
            this.rank = rank;
        }

        @Override
        public boolean is(final NativeQueue.State other) {
            return this == other;
        }

        @Override
        public boolean isAtLeast(final NativeQueue.State other) {
            if (other instanceof State) {
                return this.rank >= ((State) other).rank;
            }
            return false;
        }
    }

    private final StateHolder mStateHolder =
        new StateHolder(State.INITIAL, State.READY);

    @WrapForJNI(calledFrom = "gecko")
    private void setState(final State newState) {
        mStateHolder.setState(newState);
    }

    private final EventDispatcher mEventDispatcher = new EventDispatcher();

    private ChromeDelegate mChromeDelegate;
    /* package */ ContentListener mContentListener;
    /* package */ NavigationListener mNavigationListener;
    /* package */ ProgressListener mProgressListener;
    private InputConnectionListener mInputConnectionListener;

    private GeckoViewSettings mSettings;

    protected boolean mOnAttachedToWindowCalled;
    protected String mChromeUri;
    protected int mScreenId = 0; // default to the primary screen

    @WrapForJNI(dispatchTo = "proxy")
    protected static final class Window extends JNIObject {
        @WrapForJNI(skip = true)
        /* package */ Window() {}

        static native void open(Window instance, GeckoView view,
                                Object compositor, EventDispatcher dispatcher,
                                String chromeUri, GeckoBundle settings,
                                int screenId);

        @Override protected native void disposeNative();
        native void close();
        native void reattach(GeckoView view, Object compositor, EventDispatcher dispatcher);
        native void loadUri(String uri, int flags);
    }

    // Object to hold onto our nsWindow connection when GeckoView gets destroyed.
    private static class StateBinder extends Binder implements Parcelable {
        public final Parcelable superState;
        public final Window window;

        public StateBinder(Parcelable superState, Window window) {
            this.superState = superState;
            this.window = window;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            // Always write out the super-state, so that even if we lose this binder, we
            // will still have something to pass into super.onRestoreInstanceState.
            out.writeParcelable(superState, flags);
            out.writeStrongBinder(this);
        }

        @ReflectionTarget
        public static final Parcelable.Creator<StateBinder> CREATOR
            = new Parcelable.Creator<StateBinder>() {
                @Override
                public StateBinder createFromParcel(Parcel in) {
                    final Parcelable superState = in.readParcelable(null);
                    final IBinder binder = in.readStrongBinder();
                    if (binder instanceof StateBinder) {
                        return (StateBinder) binder;
                    }
                    // Not the original object we saved; return null state.
                    return new StateBinder(superState, null);
                }

                @Override
                public StateBinder[] newArray(int size) {
                    return new StateBinder[size];
                }
            };
    }

    private class Listener implements BundleEventListener {
        /* package */ void registerListeners() {
            getEventDispatcher().registerUiThreadListener(this,
                "GeckoView:DOMTitleChanged",
                "GeckoView:LocationChange",
                "GeckoView:PageStart",
                "GeckoView:PageStop",
                "GeckoView:SecurityChanged",
                null);
        }

        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if (DEBUG) {
                Log.d(LOGTAG, "handleMessage: event = " + event);
            }

            if ("GeckoView:DOMTitleChanged".equals(event)) {
                if (mContentListener != null) {
                    mContentListener.onTitleChanged(GeckoView.this, message.getString("title"));
                }
            } else if ("GeckoView:LocationChange".equals(event)) {
                if (mNavigationListener == null) {
                    // We shouldn't be getting this event.
                    mEventDispatcher.dispatch("GeckoViewNavigation:Inactive", null);
                } else {
                    mNavigationListener.onLocationChange(GeckoView.this, message.getString("uri"));
                    mNavigationListener.onCanGoBack(GeckoView.this, message.getBoolean("canGoBack"));
                    mNavigationListener.onCanGoForward(GeckoView.this, message.getBoolean("canGoForward"));
                }
            } else if ("GeckoView:PageStart".equals(event)) {
                if (mProgressListener != null) {
                    mProgressListener.onPageStart(GeckoView.this, message.getString("uri"));
                }
            } else if ("GeckoView:PageStop".equals(event)) {
                if (mProgressListener != null) {
                    mProgressListener.onPageStop(GeckoView.this, message.getBoolean("success"));
                }
            } else if ("GeckoView:SecurityChanged".equals(event)) {
                if (mProgressListener != null) {
                    mProgressListener.onSecurityChanged(GeckoView.this, message.getInt("status"));
                }
            }
        }
    }

    protected Window window;
    private boolean stateSaved;
    private final Listener listener = new Listener();

    public GeckoView(Context context) {
        super(context);
        init(context);
    }

    public GeckoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        if (GeckoAppShell.getApplicationContext() == null) {
            GeckoAppShell.setApplicationContext(context.getApplicationContext());
        }

        // Set the GeckoInterface if the context is an activity and the GeckoInterface
        // has not already been set
        if (context instanceof Activity && getGeckoInterface() == null) {
            setGeckoInterface(new BaseGeckoInterface(context));
            GeckoAppShell.setContextGetter(this);
        }

        // Perform common initialization for Fennec/GeckoView.
        GeckoAppShell.setLayerView(this);

        initializeView();
        listener.registerListeners();

        mSettings = new GeckoViewSettings(getEventDispatcher());
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        final Parcelable superState = super.onSaveInstanceState();
        stateSaved = true;
        return new StateBinder(superState, this.window);
    }

    @Override
    protected void onRestoreInstanceState(final Parcelable state) {
        final StateBinder stateBinder = (StateBinder) state;

        if (stateBinder.window != null) {
            this.window = stateBinder.window;
        }
        stateSaved = false;

        if (mOnAttachedToWindowCalled) {
            reattachWindow();
        }

        // We have to always call super.onRestoreInstanceState because View keeps
        // track of these calls and throws an exception when we don't call it.
        super.onRestoreInstanceState(stateBinder.superState);
    }

    protected void openWindow() {
        if (mChromeUri == null) {
            mChromeUri = getGeckoInterface().getDefaultChromeURI();
        }

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            Window.open(window, this, getCompositor(), mEventDispatcher,
                        mChromeUri, mSettings.asBundle(), mScreenId);
        } else {
            GeckoThread.queueNativeCallUntil(
                GeckoThread.State.PROFILE_READY,
                Window.class, "open", window,
                GeckoView.class, this,
                Object.class, getCompositor(),
                EventDispatcher.class, mEventDispatcher,
                String.class, mChromeUri,
                GeckoBundle.class, mSettings.asBundle(),
                mScreenId);
        }
    }

    protected void reattachWindow() {
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            window.reattach(this, getCompositor(), mEventDispatcher);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    window, "reattach", GeckoView.class, this,
                    Object.class, getCompositor(), EventDispatcher.class, mEventDispatcher);
        }
    }

    @Override
    public void onAttachedToWindow() {
        final DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();

        if (window == null) {
            // Open a new nsWindow if we didn't have one from before.
            window = new Window();
            openWindow();
        } else {
            reattachWindow();
        }

        super.onAttachedToWindow();

        mOnAttachedToWindowCalled = true;
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        super.destroy();

        if (stateSaved) {
            // If we saved state earlier, we don't want to close the nsWindow.
            return;
        }

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            window.close();
            window.disposeNative();
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    window, "close");
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    window, "disposeNative");
        }

        mOnAttachedToWindowCalled = false;
    }

    @WrapForJNI public static final int LOAD_DEFAULT = 0;
    @WrapForJNI public static final int LOAD_NEW_TAB = 1;
    @WrapForJNI public static final int LOAD_SWITCH_TAB = 2;

    /**
    * Load the given URI.
    * @param uri The URI of the resource to load.
    * @param flags The load flags (TODO).
    */
    public void loadUri(String uri, int flags) {
        if (window == null) {
            throw new IllegalStateException("Not attached to window");
        }

        if (GeckoThread.isRunning()) {
            window.loadUri(uri, flags);
        }  else {
            GeckoThread.queueNativeCall(window, "loadUri", String.class, uri, flags);
        }
    }

    /**
    * Reload the current URI.
    */
    public void reload() {
        mEventDispatcher.dispatch("GeckoView:Reload", null);
    }

    /* package */ void setInputConnectionListener(final InputConnectionListener icl) {
        mInputConnectionListener = icl;
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

    public GeckoViewSettings getSettings() {
        return mSettings;
    }

    @Override
    public Handler getHandler() {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.getHandler(super.getHandler());
        }
        return super.getHandler();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.onCreateInputConnection(outAttrs);
        }
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (super.onKeyPreIme(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (super.onKeyUp(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (super.onKeyDown(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (super.onKeyLongPress(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyLongPress(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (super.onKeyMultiple(keyCode, repeatCount, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyMultiple(keyCode, repeatCount, event);
    }

    /* package */ boolean isIMEEnabled() {
        return mInputConnectionListener != null &&
                mInputConnectionListener.isIMEEnabled();
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
    * Set the chrome callback handler.
    * This will replace the current handler.
    * @param chrome An implementation of GeckoViewChrome.
    */
    public void setChromeDelegate(ChromeDelegate chrome) {
        mChromeDelegate = chrome;
    }

    /**
    * Set the content callback handler.
    * This will replace the current handler.
    * @param content An implementation of ContentListener.
    */
    public void setContentListener(ContentListener content) {
        mContentListener = content;
    }

    /**
    * Get the content callback handler.
    * @return The current content callback handler.
    */
    public ContentListener getContentListener() {
        return mContentListener;
    }

    /**
    * Set the progress callback handler.
    * This will replace the current handler.
    * @param progress An implementation of ProgressListener.
    */
    public void setProgressListener(ProgressListener progress) {
        mProgressListener = progress;
    }

    /**
    * Get the progress callback handler.
    * @return The current progress callback handler.
    */
    public ProgressListener getProgressListener() {
        return mProgressListener;
    }

    /**
    * Set the navigation callback handler.
    * This will replace the current handler.
    * @param navigation An implementation of NavigationListener.
    */
    public void setNavigationDelegate(NavigationListener listener) {
        if (mNavigationListener == listener) {
            return;
        }
        if (listener == null) {
            mEventDispatcher.dispatch("GeckoViewNavigation:Inactive", null);
        } else if (mNavigationListener == null) {
            mEventDispatcher.dispatch("GeckoViewNavigation:Active", null);
        }

        mNavigationListener = listener;
    }

    /**
    * Get the navigation callback handler.
    * @return The current navigation callback handler.
    */
    public NavigationListener getNavigationListener() {
        return mNavigationListener;
    }

    public static void setGeckoInterface(final BaseGeckoInterface geckoInterface) {
        GeckoAppShell.setGeckoInterface(geckoInterface);
    }

    public static GeckoAppShell.GeckoInterface getGeckoInterface() {
        return GeckoAppShell.getGeckoInterface();
    }

    protected String getSharedPreferencesFile() {
        return DEFAULT_SHARED_PREFERENCES_FILE;
    }

    @Override
    public SharedPreferences getSharedPreferences() {
        return getContext().getSharedPreferences(getSharedPreferencesFile(), 0);
    }

    public EventDispatcher getEventDispatcher() {
        return mEventDispatcher;
    }

    /* Provides a means for the client to indicate whether a JavaScript
     * dialog request should proceed. An instance of this class is passed to
     * various GeckoViewChrome callback actions.
     */
    public class PromptResult {
        public PromptResult() {
        }

        /**
        * Handle a confirmation response from the user.
        */
        public void confirm() {
        }

        /**
        * Handle a confirmation response from the user.
        * @param value String value to return to the browser context.
        */
        public void confirmWithValue(String value) {
        }

        /**
        * Handle a cancellation response from the user.
        */
        public void cancel() {
        }
    }

    public interface ChromeDelegate {
        /**
        * Tell the host application to display an alert dialog.
        * @param view The GeckoView that initiated the callback.
        * @param message The string to display in the dialog.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onAlert(GeckoView view, String message, GeckoView.PromptResult result);

        /**
        * Tell the host application to display a confirmation dialog.
        * @param view The GeckoView that initiated the callback.
        * @param message The string to display in the dialog.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onConfirm(GeckoView view, String message, GeckoView.PromptResult result);

        /**
        * Tell the host application to display an input prompt dialog.
        * @param view The GeckoView that initiated the callback.
        * @param message The string to display in the dialog.
        * @param defaultValue The string to use as default input.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onPrompt(GeckoView view, String message, String defaultValue, GeckoView.PromptResult result);

        /**
        * Tell the host application to display a remote debugging request dialog.
        * @param view The GeckoView that initiated the callback.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onDebugRequest(GeckoView view, GeckoView.PromptResult result);
    }

    public interface ProgressListener {
        static final int STATE_IS_BROKEN = 1;
        static final int STATE_IS_SECURE = 2;
        static final int STATE_IS_INSECURE = 4;

        /**
        * A View has started loading content from the network.
        * @param view The GeckoView that initiated the callback.
        * @param url The resource being loaded.
        */
        public void onPageStart(GeckoView view, String url);

        /**
        * A View has finished loading content from the network.
        * @param view The GeckoView that initiated the callback.
        * @param success Whether the page loaded successfully or an error occurred.
        */
        public void onPageStop(GeckoView view, boolean success);

        /**
        * The security status has been updated.
        * @param view The GeckoView that initiated the callback.
        * @param status The new security status.
        */
        public void onSecurityChanged(GeckoView view, int status);
    }

    public interface ContentListener {
        /**
        * A page title was discovered in the content or updated after the content
        * loaded.
        * @param view The GeckoView that initiated the callback.
        * @param title The title sent from the content.
        */
        public void onTitleChanged(GeckoView view, String title);
    }

    public interface NavigationListener {
        /**
        * A view has started loading content from the network.
        * @param view The GeckoView that initiated the callback.
        * @param url The resource being loaded.
        */
        public void onLocationChange(GeckoView view, String url);

        /**
        * The view's ability to go back has changed.
        * @param view The GeckoView that initiated the callback.
        * @param canGoBack The new value for the ability.
        */
        public void onCanGoBack(GeckoView view, boolean canGoBack);

        /**
        * The view's ability to go forward has changed.
        * @param view The GeckoView that initiated the callback.
        * @param canGoForward The new value for the ability.
        */
        public void onCanGoForward(GeckoView view, boolean canGoForward);
    }
}
