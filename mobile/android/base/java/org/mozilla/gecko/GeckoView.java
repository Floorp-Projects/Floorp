/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.GLController;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.TypedArray;
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

    private ChromeDelegate mChromeDelegate;
    private ContentDelegate mContentDelegate;

    private InputConnectionListener mInputConnectionListener;

    private final GeckoEventListener mGeckoEventListener = new GeckoEventListener() {
        @Override
        public void handleMessage(final String event, final JSONObject message) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (event.equals("Gecko:Ready")) {
                            handleReady(message);
                        } else if (event.equals("Content:StateChange")) {
                            handleStateChange(message);
                        } else if (event.equals("Content:LoadError")) {
                            handleLoadError(message);
                        } else if (event.equals("Content:PageShow")) {
                            handlePageShow(message);
                        } else if (event.equals("DOMTitleChanged")) {
                            handleTitleChanged(message);
                        } else if (event.equals("Link:Favicon")) {
                            handleLinkFavicon(message);
                        } else if (event.equals("Prompt:Show") || event.equals("Prompt:ShowTop")) {
                            handlePrompt(message);
                        } else if (event.equals("Accessibility:Event")) {
                            int mode = getImportantForAccessibility();
                            if (mode == View.IMPORTANT_FOR_ACCESSIBILITY_YES ||
                                mode == View.IMPORTANT_FOR_ACCESSIBILITY_AUTO) {
                                GeckoAccessibility.sendAccessibilityEvent(message);
                            }
                        }
                    } catch (Exception e) {
                        Log.e(LOGTAG, "handleMessage threw for " + event, e);
                    }
                }
            });
        }
    };

    private final NativeEventListener mNativeEventListener = new NativeEventListener() {
        @Override
        public void handleMessage(final String event, final NativeJSObject message, final EventCallback callback) {
            try {
                if ("Accessibility:Ready".equals(event)) {
                    GeckoAccessibility.updateAccessibilitySettings(getContext());
                } else if ("GeckoView:Message".equals(event)) {
                    // We need to pull out the bundle while on the Gecko thread.
                    NativeJSObject json = message.optObject("data", null);
                    if (json == null) {
                        // Must have payload to call the message handler.
                        return;
                    }
                    final Bundle data = json.toBundle();
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            handleScriptMessage(data, callback);
                        }
                    });
                }
            } catch (Exception e) {
                Log.w(LOGTAG, "handleMessage threw for " + event, e);
            }
        }
    };

    @WrapForJNI
    private static final class Window extends JNIObject {
        /* package */ final GLController glController = new GLController();

        static native void open(Window instance, GeckoView view, GLController glController,
                                int width, int height);

        @Override protected native void disposeNative();
        native void close();
        native void reattach(GeckoView view);
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

    private Window window;
    private boolean stateSaved;

    public GeckoView(Context context) {
        super(context);
        init(context, null, true);
    }

    public GeckoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.GeckoView);
        String url = a.getString(R.styleable.GeckoView_url);
        boolean doInit = a.getBoolean(R.styleable.GeckoView_doinit, true);
        a.recycle();
        init(context, url, doInit);
    }

    private void init(Context context, String url, boolean doInit) {

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

        initializeView(EventDispatcher.getInstance());

        // TODO: Fennec currently takes care of its own initialization, so this
        // flag is a hack used in Fennec to prevent GeckoView initialization.
        // This should go away once Fennec also uses GeckoView for
        // initialization.
        if (!doInit)
            return;

        // If running outside of a GeckoActivity (eg, from a library project),
        // load the native code and disable content providers
        boolean isGeckoActivity = false;
        try {
            isGeckoActivity = context instanceof GeckoActivity;
        } catch (NoClassDefFoundError ex) {}

        if (!isGeckoActivity) {
            Clipboard.init(context);
            HardwareUtils.init(context);

            // If you want to use GeckoNetworkManager, start it.

            final GeckoProfile profile = GeckoProfile.get(context);
         }

        GeckoThread.ensureInit(null, null);
        if (url != null) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(url));
        }

        if (context instanceof Activity) {
            Tabs tabs = Tabs.getInstance();
            tabs.attachToContext(context);
        }

        EventDispatcher.getInstance().registerGeckoThreadListener(mGeckoEventListener,
            "Gecko:Ready",
            "Accessibility:Event",
            "Content:StateChange",
            "Content:LoadError",
            "Content:PageShow",
            "DOMTitleChanged",
            "Link:Favicon",
            "Prompt:Show",
            "Prompt:ShowTop");

        EventDispatcher.getInstance().registerGeckoThreadListener(mNativeEventListener,
            "Accessibility:Ready",
            "GeckoView:Message");

        if (GeckoThread.launch()) {
            // This is the first launch, so finish initialization and go.
            GeckoProfile profile = GeckoProfile.get(context).forceCreate();

        } else if (GeckoThread.isRunning()) {
            // If Gecko is already running, that means the Activity was
            // destroyed, so we need to re-attach Gecko to this GeckoView.
            connectToGecko();
        }
    }

    @Override
    protected Parcelable onSaveInstanceState()
    {
        final Parcelable superState = super.onSaveInstanceState();
        stateSaved = true;
        return new StateBinder(superState, this.window);
    }

    @Override
    protected void onRestoreInstanceState(final Parcelable state)
    {
        final StateBinder stateBinder = (StateBinder) state;
        // We have to always call super.onRestoreInstanceState because View keeps
        // track of these calls and throws an exception when we don't call it.
        super.onRestoreInstanceState(stateBinder.superState);

        if (stateBinder.window != null) {
            this.window = stateBinder.window;
        }
        stateSaved = false;
    }

    @Override
    public void onAttachedToWindow()
    {
        final DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();

        if (window == null) {
            // Open a new nsWindow if we didn't have one from before.
            window = new Window();

            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                Window.open(window, this, window.glController,
                            metrics.widthPixels, metrics.heightPixels);
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY, Window.class,
                        "open", window, GeckoView.class, this, window.glController,
                        metrics.widthPixels, metrics.heightPixels);
            }
        } else {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                window.reattach(this);
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                        window, "reattach", GeckoView.class, this);
            }
        }

        setGLController(window.glController);
        super.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow()
    {
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
    }

    private int mLastVisibility = View.GONE;

    /* package */ void setInputConnectionListener(final InputConnectionListener icl) {
        mInputConnectionListener = icl;
        if (mInputConnectionListener != null) {
            mInputConnectionListener.onWindowVisibilityChanged(mLastVisibility);
        }
    }

    @Override
    protected void onWindowVisibilityChanged (int visibility) {
        mLastVisibility = visibility;

        if (mInputConnectionListener != null) {
            mInputConnectionListener.onWindowVisibilityChanged(visibility);
        }

        super.onWindowVisibilityChanged(visibility);
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

    /**
    * Add a Browser to the GeckoView container.
    * @param url The URL resource to load into the new Browser.
    */
    public Browser addBrowser(String url) {
        Tab tab = Tabs.getInstance().loadUrl(url, Tabs.LOADURL_NEW_TAB);
        if (tab != null) {
            return new Browser(tab.getId());
        }
        return null;
    }

    /**
    * Remove a Browser from the GeckoView container.
    * @param browser The Browser to remove.
    */
    public void removeBrowser(Browser browser) {
        Tab tab = Tabs.getInstance().getTab(browser.getId());
        if (tab != null) {
            Tabs.getInstance().closeTab(tab);
        }
    }

    /**
    * Set the active/visible Browser.
    * @param browser The Browser to make selected.
    */
    public void setCurrentBrowser(Browser browser) {
        Tab tab = Tabs.getInstance().getTab(browser.getId());
        if (tab != null) {
            Tabs.getInstance().selectTab(tab.getId());
        }
    }

    /**
    * Get the active/visible Browser.
    * @return The current selected Browser.
    */
    public Browser getCurrentBrowser() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            return new Browser(tab.getId());
        }
        return null;
    }

    /**
    * Get the list of current Browsers in the GeckoView container.
    * @return An unmodifiable List of Browser objects.
    */
    public List<Browser> getBrowsers() {
        ArrayList<Browser> browsers = new ArrayList<Browser>();
        Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : tabs) {
            browsers.add(new Browser(tab.getId()));
        }
        return Collections.unmodifiableList(browsers);
    }

    public void importScript(final String url) {
        if (url.startsWith("resource://android/assets/")) {
            GeckoAppShell.notifyObservers("GeckoView:ImportScript", url);
            return;
        }

        throw new IllegalArgumentException("Must import script from 'resources://android/assets/' location.");
    }

    private void connectToGecko() {
        Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null) {
            Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
        }

        GeckoAppShell.notifyObservers("Viewport:Flush", null);
    }

    private void handleReady(final JSONObject message) {
        connectToGecko();

        if (mChromeDelegate != null) {
            mChromeDelegate.onReady(this);
        }
    }

    private void handleStateChange(final JSONObject message) throws JSONException {
        int state = message.getInt("state");
        if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) != 0) {
            if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                if (mContentDelegate != null) {
                    int id = message.getInt("tabID");
                    mContentDelegate.onPageStart(this, new Browser(id), message.getString("uri"));
                }
            } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                if (mContentDelegate != null) {
                    int id = message.getInt("tabID");
                    mContentDelegate.onPageStop(this, new Browser(id), message.getBoolean("success"));
                }
            }
        }
    }

    private void handleLoadError(final JSONObject message) throws JSONException {
        if (mContentDelegate != null) {
            int id = message.getInt("tabID");
            mContentDelegate.onPageStop(GeckoView.this, new Browser(id), false);
        }
    }

    private void handlePageShow(final JSONObject message) throws JSONException {
        if (mContentDelegate != null) {
            int id = message.getInt("tabID");
            mContentDelegate.onPageShow(GeckoView.this, new Browser(id));
        }
    }

    private void handleTitleChanged(final JSONObject message) throws JSONException {
        if (mContentDelegate != null) {
            int id = message.getInt("tabID");
            mContentDelegate.onReceivedTitle(GeckoView.this, new Browser(id), message.getString("title"));
        }
    }

    private void handleLinkFavicon(final JSONObject message) throws JSONException {
        if (mContentDelegate != null) {
            int id = message.getInt("tabID");
            mContentDelegate.onReceivedFavicon(GeckoView.this, new Browser(id), message.getString("href"), message.getInt("size"));
        }
    }

    private void handlePrompt(final JSONObject message) throws JSONException {
        if (mChromeDelegate != null) {
            String hint = message.optString("hint");
            if ("alert".equals(hint)) {
                String text = message.optString("text");
                mChromeDelegate.onAlert(GeckoView.this, null, text, new PromptResult(message));
            } else if ("confirm".equals(hint)) {
                String text = message.optString("text");
                mChromeDelegate.onConfirm(GeckoView.this, null, text, new PromptResult(message));
            } else if ("prompt".equals(hint)) {
                String text = message.optString("text");
                String defaultValue = message.optString("textbox0");
                mChromeDelegate.onPrompt(GeckoView.this, null, text, defaultValue, new PromptResult(message));
            } else if ("remotedebug".equals(hint)) {
                mChromeDelegate.onDebugRequest(GeckoView.this, new PromptResult(message));
            }
        }
    }

    private void handleScriptMessage(final Bundle data, final EventCallback callback) {
        if (mChromeDelegate != null) {
            MessageResult result = null;
            if (callback != null) {
                result = new MessageResult(callback);
            }
            mChromeDelegate.onScriptMessage(GeckoView.this, data, result);
        }
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
    * @param content An implementation of ContentDelegate.
    */
    public void setContentDelegate(ContentDelegate content) {
        mContentDelegate = content;
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

    /**
    * Wrapper for a browser in the GeckoView container. Associated with a browser
    * element in the Gecko system.
    */
    public class Browser {
        private final int mId;
        private Browser(int Id) {
            mId = Id;
        }

        /**
        * Get the ID of the Browser. This is the same ID used by Gecko for it's underlying
        * browser element.
        * @return The integer ID of the Browser.
        */
        private int getId() {
            return mId;
        }

        /**
        * Load a URL resource into the Browser.
        * @param url The URL string.
        */
        public void loadUrl(String url) {
            JSONObject args = new JSONObject();
            try {
                args.put("url", url);
                args.put("parentId", -1);
                args.put("newTab", false);
                args.put("tabID", mId);
            } catch (Exception e) {
                Log.w(LOGTAG, "Error building JSON arguments for loadUrl.", e);
            }
            GeckoAppShell.notifyObservers("Tab:Load", args.toString());
        }

        /**
        * Reload the current URL resource into the Browser. The URL is force loaded from the
        * network and is not pulled from cache.
        */
        public void reload() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                tab.doReload(true);
            }
        }

        /**
        * Stop the current loading operation.
        */
        public void stop() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                tab.doStop();
            }
        }

        /**
        * Check to see if the Browser has session history and can go back to a
        * previous page.
        * @return A boolean flag indicating if previous session exists.
        * This method will likely be removed and replaced by a callback in GeckoViewContent
        */
        public boolean canGoBack() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                return tab.canDoBack();
            }
            return false;
        }

        /**
        * Move backward in the session history, if that's possible.
        */
        public void goBack() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                tab.doBack();
            }
        }

        /**
        * Check to see if the Browser has session history and can go forward to a
        * new page.
        * @return A boolean flag indicating if forward session exists.
        * This method will likely be removed and replaced by a callback in GeckoViewContent
        */
        public boolean canGoForward() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                return tab.canDoForward();
            }
            return false;
        }

        /**
        * Move forward in the session history, if that's possible.
        */
        public void goForward() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                tab.doForward();
            }
        }
    }

    /* Provides a means for the client to indicate whether a JavaScript
     * dialog request should proceed. An instance of this class is passed to
     * various GeckoViewChrome callback actions.
     */
    public class PromptResult {
        private final int RESULT_OK = 0;
        private final int RESULT_CANCEL = 1;

        private final JSONObject mMessage;

        public PromptResult(JSONObject message) {
            mMessage = message;
        }

        private JSONObject makeResult(int resultCode) {
            JSONObject result = new JSONObject();
            try {
                result.put("button", resultCode);
            } catch(JSONException ex) { }
            return result;
        }

        /**
        * Handle a confirmation response from the user.
        */
        public void confirm() {
            JSONObject result = makeResult(RESULT_OK);
            EventDispatcher.sendResponse(mMessage, result);
        }

        /**
        * Handle a confirmation response from the user.
        * @param value String value to return to the browser context.
        */
        public void confirmWithValue(String value) {
            JSONObject result = makeResult(RESULT_OK);
            try {
                result.put("textbox0", value);
            } catch(JSONException ex) { }
            EventDispatcher.sendResponse(mMessage, result);
        }

        /**
        * Handle a cancellation response from the user.
        */
        public void cancel() {
            JSONObject result = makeResult(RESULT_CANCEL);
            EventDispatcher.sendResponse(mMessage, result);
        }
    }

    /* Provides a means for the client to respond to a script message with some data.
     * An instance of this class is passed to GeckoViewChrome.onScriptMessage.
     */
    public class MessageResult {
        private final EventCallback mCallback;

        public MessageResult(EventCallback callback) {
            if (callback == null) {
                throw new IllegalArgumentException("EventCallback should not be null.");
            }
            mCallback = callback;
        }

        private JSONObject bundleToJSON(Bundle data) {
            JSONObject result = new JSONObject();
            if (data == null) {
                return result;
            }

            final Set<String> keys = data.keySet();
            for (String key : keys) {
                try {
                    result.put(key, data.get(key));
                } catch (JSONException e) {
                }
            }
            return result;
        }

        /**
        * Handle a successful response to a script message.
        * @param value Bundle value to return to the script context.
        */
        public void success(Bundle data) {
            mCallback.sendSuccess(bundleToJSON(data));
        }

        /**
        * Handle a failure response to a script message.
        */
        public void failure(Bundle data) {
            mCallback.sendError(bundleToJSON(data));
        }
    }

    public interface ChromeDelegate {
        /**
        * Tell the host application that Gecko is ready to handle requests.
        * @param view The GeckoView that initiated the callback.
        */
        public void onReady(GeckoView view);

        /**
        * Tell the host application to display an alert dialog.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is loading the content.
        * @param message The string to display in the dialog.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onAlert(GeckoView view, GeckoView.Browser browser, String message, GeckoView.PromptResult result);
    
        /**
        * Tell the host application to display a confirmation dialog.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is loading the content.
        * @param message The string to display in the dialog.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onConfirm(GeckoView view, GeckoView.Browser browser, String message, GeckoView.PromptResult result);
    
        /**
        * Tell the host application to display an input prompt dialog.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is loading the content.
        * @param message The string to display in the dialog.
        * @param defaultValue The string to use as default input.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onPrompt(GeckoView view, GeckoView.Browser browser, String message, String defaultValue, GeckoView.PromptResult result);
    
        /**
        * Tell the host application to display a remote debugging request dialog.
        * @param view The GeckoView that initiated the callback.
        * @param result A PromptResult used to send back the result without blocking.
        * Defaults to cancel requests.
        */
        public void onDebugRequest(GeckoView view, GeckoView.PromptResult result);

        /**
        * Receive a message from an imported script.
        * @param view The GeckoView that initiated the callback.
        * @param data Bundle of data sent with the message. Never null.
        * @param result A MessageResult used to send back a response without blocking. Can be null.
        * Defaults to do nothing.
        */
        public void onScriptMessage(GeckoView view, Bundle data, GeckoView.MessageResult result);
    }

    public interface ContentDelegate {
        /**
        * A Browser has started loading content from the network.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is loading the content.
        * @param url The resource being loaded.
        */
        public void onPageStart(GeckoView view, GeckoView.Browser browser, String url);
    
        /**
        * A Browser has finished loading content from the network.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that was loading the content.
        * @param success Whether the page loaded successfully or an error occurred.
        */
        public void onPageStop(GeckoView view, GeckoView.Browser browser, boolean success);

        /**
        * A Browser is displaying content. This page could have been loaded via
        * network or from the session history.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is showing the content.
        */
        public void onPageShow(GeckoView view, GeckoView.Browser browser);

        /**
        * A page title was discovered in the content or updated after the content
        * loaded.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is showing the content.
        * @param title The title sent from the content.
        */
        public void onReceivedTitle(GeckoView view, GeckoView.Browser browser, String title);

        /**
        * A link element was discovered in the content or updated after the content
        * loaded that specifies a favicon.
        * @param view The GeckoView that initiated the callback.
        * @param browser The Browser that is showing the content.
        * @param url The href of the link element specifying the favicon.
        * @param size The maximum size specified for the favicon, or -1 for any size.
        */
        public void onReceivedFavicon(GeckoView view, GeckoView.Browser browser, String url, int size);
    }

}
