/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.TypedArray;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class GeckoView extends LayerView
    implements GeckoEventListener, ContextGetter {

    private static final String DEFAULT_SHARED_PREFERENCES_FILE = "GeckoView";
    private static final String LOGTAG = "GeckoView";

    private ChromeDelegate mChromeDelegate;
    private ContentDelegate mContentDelegate;

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
            // Set the GeckoInterface if the context is an activity and the GeckoInterface
            // has not already been set
            if (context instanceof Activity && getGeckoInterface() == null) {
                setGeckoInterface(new BaseGeckoInterface(context));
            }

            Clipboard.init(context);
            HardwareUtils.init(context);

            // If you want to use GeckoNetworkManager, start it.

            GeckoLoader.loadMozGlue(context);
            BrowserDB.setEnableContentProviders(false);
         }

        if (url != null) {
            GeckoThread.setUri(url);
            GeckoThread.setAction(Intent.ACTION_VIEW);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(url));
        }
        GeckoAppShell.setContextGetter(this);
        if (context instanceof Activity) {
            Tabs tabs = Tabs.getInstance();
            tabs.attachToContext(context);
        }

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Gecko:Ready",
            "Content:StateChange",
            "Content:LoadError",
            "Content:PageShow",
            "DOMTitleChanged",
            "Link:Favicon",
            "Prompt:Show",
            "Prompt:ShowTop");

        ThreadUtils.setUiThread(Thread.currentThread(), new Handler());
        initializeView(EventDispatcher.getInstance());

        if (GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.Launched)) {
            // This is the first launch, so finish initialization and go.
            GeckoProfile profile = GeckoProfile.get(context).forceCreate();
            BrowserDB.initialize(profile.getName());

            GeckoAppShell.setLayerView(this);
            GeckoThread.createAndStart();
        } else if(GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            // If Gecko is already running, that means the Activity was
            // destroyed, so we need to re-attach Gecko to this GeckoView.
            connectToGecko();
        }
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

    /**
    * Not part of the public API. Ignore.
    */
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (event.equals("Gecko:Ready")) {
                        GeckoView.this.handleReady(message);
                    } else if (event.equals("Content:StateChange")) {
                        GeckoView.this.handleStateChange(message);
                    } else if (event.equals("Content:LoadError")) {
                        GeckoView.this.handleLoadError(message);
                    } else if (event.equals("Content:PageShow")) {
                        GeckoView.this.handlePageShow(message);
                    } else if (event.equals("DOMTitleChanged")) {
                        GeckoView.this.handleTitleChanged(message);
                    } else if (event.equals("Link:Favicon")) {
                        GeckoView.this.handleLinkFavicon(message);
                    } else if (event.equals("Prompt:Show") || event.equals("Prompt:ShowTop")) {
                        GeckoView.this.handlePrompt(message);
                    }
                } catch (Exception e) {
                    Log.w(LOGTAG, "handleMessage threw for " + event, e);
                }
            }
        });
    }

    private void connectToGecko() {
        GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoRunning);
        Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null)
            Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
        geckoConnected();
        GeckoAppShell.setLayerClient(getLayerClientObject());
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Viewport:Flush", null));
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
                mChromeDelegate.onAlert(GeckoView.this, null, text, new PromptResult(message.optString("guid")));
            } else if ("confirm".equals(hint)) {
                String text = message.optString("text");
                mChromeDelegate.onConfirm(GeckoView.this, null, text, new PromptResult(message.optString("guid")));
            } else if ("prompt".equals(hint)) {
                String text = message.optString("text");
                String defaultValue = message.optString("textbox0");
                mChromeDelegate.onPrompt(GeckoView.this, null, text, defaultValue, new PromptResult(message.optString("guid")));
            } else if ("remotedebug".equals(hint)) {
                mChromeDelegate.onDebugRequest(GeckoView.this, new PromptResult(message.optString("guid")));
            }
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
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Load", args.toString()));
        }

        /**
        * Reload the current URL resource into the Browser. The URL is force loaded from the
        * network and is not pulled from cache.
        */
        public void reload() {
            Tab tab = Tabs.getInstance().getTab(mId);
            if (tab != null) {
                tab.doReload();
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

        private final String mGUID;

        public PromptResult(String guid) {
            mGUID = guid;
        }

        private JSONObject makeResult(int resultCode) {
            JSONObject result = new JSONObject();
            try {
                result.put("guid", mGUID);
                result.put("button", resultCode);
            } catch(JSONException ex) { }
            return result;
        }

        /**
        * Handle a confirmation response from the user.
        */
        public void confirm() {
            JSONObject result = makeResult(RESULT_OK);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Prompt:Reply", result.toString()));
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
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Prompt:Reply", result.toString()));
        }

        /**
        * Handle a cancellation response from the user.
        */
        public void cancel() {
            JSONObject result = makeResult(RESULT_CANCEL);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Prompt:Reply", result.toString()));
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
        * @param success Whether the page loaded successfully or an error occured.
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
