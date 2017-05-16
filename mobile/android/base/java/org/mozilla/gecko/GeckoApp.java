/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.FullScreenState;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.health.HealthRecorder;
import org.mozilla.gecko.health.SessionInformation;
import org.mozilla.gecko.health.StubbedHealthRecorder;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.menu.MenuPanel;
import org.mozilla.gecko.mma.MmaDelegate;
import org.mozilla.gecko.notifications.NotificationHelper;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.preferences.ClearOnShutdownPref;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.tabqueue.TabQueueHelper;
import org.mozilla.gecko.text.FloatingToolbarTextSelection;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.PrefUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.gecko.widget.AnchoredPopup;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.StrictMode;
import android.provider.ContactsContract;
import android.provider.MediaStore.Images.Media;
import android.support.annotation.NonNull;
import android.support.annotation.WorkerThread;
import android.support.design.widget.Snackbar;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Base64;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.SimpleAdapter;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import static org.mozilla.gecko.Tabs.INTENT_EXTRA_SESSION_UUID;
import static org.mozilla.gecko.Tabs.INTENT_EXTRA_TAB_ID;
import static org.mozilla.gecko.Tabs.INVALID_TAB_ID;
import static org.mozilla.gecko.mma.MmaDelegate.DOWNLOAD_VIDEOS_OR_ANY_OTHER_MEDIA;
import static org.mozilla.gecko.mma.MmaDelegate.LOADS_ARTICLES;

public abstract class GeckoApp extends GeckoActivity
                               implements AnchoredPopup.OnVisibilityChangeListener,
                                          BundleEventListener,
                                          ContextGetter,
                                          GeckoMenu.Callback,
                                          GeckoMenu.MenuPresenter,
                                          GeckoView.ContentListener,
                                          ScreenOrientationDelegate,
                                          Tabs.OnTabsChangedListener,
                                          ViewTreeObserver.OnGlobalLayoutListener {

    private static final String LOGTAG = "GeckoApp";
    private static final long ONE_DAY_MS = TimeUnit.MILLISECONDS.convert(1, TimeUnit.DAYS);

    public static final String ACTION_ALERT_CALLBACK       = "org.mozilla.gecko.ALERT_CALLBACK";
    public static final String ACTION_HOMESCREEN_SHORTCUT  = "org.mozilla.gecko.BOOKMARK";
    public static final String ACTION_WEBAPP               = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG                = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_LAUNCH_SETTINGS      = "org.mozilla.gecko.SETTINGS";
    public static final String ACTION_LOAD                 = "org.mozilla.gecko.LOAD";
    public static final String ACTION_INIT_PW              = "org.mozilla.gecko.INIT_PW";
    public static final String ACTION_SWITCH_TAB           = "org.mozilla.gecko.SWITCH_TAB";

    public static final String INTENT_REGISTER_STUMBLER_LISTENER = "org.mozilla.gecko.STUMBLER_REGISTER_LOCAL_LISTENER";

    public static final String EXTRA_STATE_BUNDLE          = "stateBundle";

    protected static final String LAST_SELECTED_TAB        = "lastSelectedTab";
    protected static final String LAST_SESSION_UUID        = "lastSessionUUID";
    protected static final String STARTUP_SELECTED_TAB     = "restoredSelectedTab";
    protected static final String STARTUP_SESSION_UUID     = "restorationSessionUUID";

    public static final String PREFS_ALLOW_STATE_BUNDLE    = "allowStateBundle";
    public static final String PREFS_FLASH_USAGE           = "playFlashCount";
    public static final String PREFS_VERSION_CODE          = "versionCode";
    public static final String PREFS_WAS_STOPPED           = "wasStopped";
    public static final String PREFS_CRASHED_COUNT         = "crashedCount";
    public static final String PREFS_CLEANUP_TEMP_FILES    = "cleanupTempFiles";

    // Originally, this was only used for the telemetry core ping logic. To avoid
    // having to write custom migration logic, we just keep the original pref key.
    public static final String PREFS_IS_FIRST_RUN = "telemetry-isFirstRun";

    public static final String SAVED_STATE_IN_BACKGROUND   = "inBackground";
    public static final String SAVED_STATE_PRIVATE_SESSION = "privateSession";

    // Delay before running one-time "cleanup" tasks that may be needed
    // after a version upgrade.
    private static final int CLEANUP_DEFERRAL_SECONDS = 15;

    // Length of time in ms during which crashes are classified as startup crashes
    // for crash loop detection purposes.
    private static final int STARTUP_PHASE_DURATION_MS = 30 * 1000;

    private static boolean sAlreadyLoaded;

    protected boolean mIgnoreLastSelectedTab;
    protected static WeakReference<GeckoApp> mLastActiveGeckoApp;

    protected RelativeLayout mRootLayout;
    protected RelativeLayout mMainLayout;

    protected RelativeLayout mGeckoLayout;
    private OrientationEventListener mCameraOrientationEventListener;
    protected MenuPanel mMenuPanel;
    protected Menu mMenu;
    protected boolean mIsRestoringActivity;

    /** Tells if we're aborting app launch, e.g. if this is an unsupported device configuration. */
    protected boolean mIsAbortingAppLaunch;

    private PromptService mPromptService;
    protected TextSelection mTextSelection;

    protected DoorHangerPopup mDoorHangerPopup;
    protected FormAssistPopup mFormAssistPopup;

    protected GeckoView mLayerView;

    private FullScreenHolder mFullScreenPluginContainer;
    private View mFullScreenPluginView;

    protected boolean mLastSessionCrashed;
    protected boolean mShouldRestore;
    private boolean mSessionRestoreParsingFinished = false;

    protected int mLastSelectedTabId = INVALID_TAB_ID;
    protected String mLastSessionUUID = null;
    protected boolean mSuppressActivitySwitch = false;

    private boolean foregrounded = false;

    private static final class LastSessionParser extends SessionParser {
        private JSONArray tabs;
        private JSONObject windowObject;
        private boolean loadingExternalURL;

        private int selectedTabId = INVALID_TAB_ID;

        private boolean selectNextTab;
        private boolean tabsWereSkipped;
        private boolean tabsWereProcessed;

        private SparseIntArray tabIdMap;

        /**
         * @param loadingExternalURL Pass true if we're going to open an additional tab to load an
         *                           URL received through our launch intent.
         */
        public LastSessionParser(JSONArray tabs, JSONObject windowObject, boolean loadingExternalURL) {
            this.tabs = tabs;
            this.windowObject = windowObject;
            this.loadingExternalURL = loadingExternalURL;

            tabIdMap = new SparseIntArray();
        }

        public boolean allTabsSkipped() {
            return tabsWereSkipped && !tabsWereProcessed;
        }

        public int getNewTabId(int oldTabId) {
            return tabIdMap.get(oldTabId, INVALID_TAB_ID);
        }

        /**
         * @return The index of the tab that should be selected according to the session store data.
         *         In conjunction with opening external tabs, this might not be the tab that
         *         actually gets selected in the end, though.
         */
        public int getStoredSelectedTabId() {
            return selectedTabId;
        }

        @Override
        public void onTabRead(final SessionTab sessionTab) {
            if (sessionTab.isAboutHomeWithoutHistory()) {
                // This is a tab pointing to about:home with no history. We won't restore
                // this tab. If we end up restoring no tabs then the browser will decide
                // whether it needs to open about:home or a different 'homepage'. If we'd
                // always restore about:home only tabs then we'd never open the homepage.
                // See bug 1261008.

                if (!loadingExternalURL && sessionTab.isSelected()) {
                    // Unfortunately this tab is the selected tab. Let's just try to select
                    // the first tab. If we haven't restored any tabs so far then remember
                    // to select the next tab that gets restored.

                    if (!Tabs.getInstance().selectLastTab()) {
                        selectNextTab = true;
                    }
                }

                // Do not restore this tab.
                tabsWereSkipped = true;
                return;
            }

            tabsWereProcessed = true;

            JSONObject tabObject = sessionTab.getTabObject();

            int flags = Tabs.LOADURL_NEW_TAB;
            flags |= ((loadingExternalURL || !sessionTab.isSelected()) ? Tabs.LOADURL_DELAY_LOAD : 0);
            flags |= (tabObject.optBoolean("desktopMode") ? Tabs.LOADURL_DESKTOP : 0);
            flags |= (tabObject.optBoolean("isPrivate") ? Tabs.LOADURL_PRIVATE : 0);

            final Tab tab = Tabs.getInstance().loadUrl(sessionTab.getUrl(), flags);

            if (sessionTab.isSelected() || selectNextTab) {
                selectedTabId = tab.getId();
            }
            if (selectNextTab) {
                // We did not restore the selected tab previously. Now let's select this tab.
                Tabs.getInstance().selectTab(tab.getId());
                selectNextTab = false;
            }

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    tab.updateTitle(sessionTab.getTitle());
                }
            });

            try {
                int oldTabId = tabObject.optInt("tabId", INVALID_TAB_ID);
                int newTabId = tab.getId();
                tabObject.put("tabId", newTabId);
                if  (oldTabId >= 0) {
                    tabIdMap.put(oldTabId, newTabId);
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "JSON error", e);
            }
            tabs.put(tabObject);
        }

        @Override
        public void onClosedTabsRead(final JSONArray closedTabData) throws JSONException {
            windowObject.put("closedTabs", closedTabData);
        }

        /**
         * Updates stored parent tab IDs in the session store data to match the new tab IDs
         * that have been allocated during startup session restore.
         *
         * @param tabData A JSONArray containg stored session store tabs.
         */
        public void updateParentId(final JSONArray tabData) {
            if (tabData == null) {
                return;
            }

            for (int i = 0; i < tabData.length(); i++) {
                try {
                    JSONObject tabObject = tabData.getJSONObject(i);

                    int parentId = tabObject.getInt("parentId");
                    int newParentId = getNewTabId(parentId);

                    tabObject.put("parentId", newParentId);
                } catch (JSONException ex) {
                    // Tabs are not guaranteed to have a parentId,
                    // so just skip the tab and try the next one.
                }
            }
        }
    };

    protected boolean mInitialized;
    protected boolean mWindowFocusInitialized;
    private Telemetry.Timer mJavaUiStartupTimer;
    private Telemetry.Timer mGeckoReadyStartupTimer;

    private String mPrivateBrowsingSession;

    private volatile HealthRecorder mHealthRecorder;
    private volatile Locale mLastLocale;

    private boolean mShutdownOnDestroy;
    private boolean mRestartOnShutdown;

    private boolean mWasFirstTabShownAfterActivityUnhidden;

    abstract public int getLayout();

    abstract public View getDoorhangerOverlay();

    protected void processTabQueue() {};

    protected void openQueuedTabs() {};

    @SuppressWarnings("serial")
    class SessionRestoreException extends Exception {
        public SessionRestoreException(Exception e) {
            super(e);
        }

        public SessionRestoreException(String message) {
            super(message);
        }
    }

    void toggleChrome(final boolean aShow) { }

    void focusChrome() { }

    @Override
    public Context getContext() {
        return this;
    }

    @Override
    public SharedPreferences getSharedPreferences() {
        return GeckoSharedPrefs.forApp(this);
    }

    public SharedPreferences getSharedPreferencesForProfile() {
        return GeckoSharedPrefs.forProfile(this);
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        // When a tab is closed, it is always unselected first.
        // When a tab is unselected, another tab is always selected first.
        switch (msg) {
            case UNSELECTED:
                break;

            case LOCATION_CHANGE:
                // We only care about location change for the selected tab.
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    resetOptionsMenu();
                    resetFormAssistPopup();
                }
                break;

            case SELECTED:
                resetOptionsMenu();
                resetFormAssistPopup();

                if (foregrounded) {
                    tab.setWasSelectedInForeground(true);
                }

                if (mLastSelectedTabId != INVALID_TAB_ID && foregrounded &&
                        // mSuppressActivitySwitch implies that we want to defer a pending
                        // activity switch because we're actually about to leave the app.
                        !mSuppressActivitySwitch && !tab.matchesActivity(this)) {
                    startActivity(IntentHelper.getTabSwitchIntent(tab));
                } else if (saveAsLastSelectedTab(tab)) {
                    mLastSelectedTabId = tab.getId();
                    mLastSessionUUID = GeckoApplication.getSessionUUID();
                }
                break;

            case CLOSED:
                if (saveAsLastSelectedTab(tab)) {
                    if (mLastSelectedTabId == tab.getId() &&
                            GeckoApplication.getSessionUUID().equals(mLastSessionUUID)) {
                        mLastSelectedTabId = Tabs.INVALID_TAB_ID;
                        mLastSessionUUID = null;
                    }
                }
                break;

            case DESKTOP_MODE_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab))
                    resetOptionsMenu();
                break;
        }
    }

    private void resetOptionsMenu() {
        if (mInitialized) {
            invalidateOptionsMenu();
        }
    }

    private void resetFormAssistPopup() {
        if (mInitialized && mFormAssistPopup != null) {
            mFormAssistPopup.hide();
        }
    }

    /**
     * Called on tab selection and tab close - return true to allow updating of this activity's
     * last selected tab.
     */
    protected boolean saveAsLastSelectedTab(Tab tab) {
        return false;
    }

    public void refreshChrome() {
    }

    public void invalidateOptionsMenu() {
        if (mMenu == null) {
            return;
        }

        onPrepareOptionsMenu(mMenu);

        super.invalidateOptionsMenu();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        mMenu = menu;

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.gecko_app_menu, mMenu);
        return true;
    }

    @Override
    public MenuInflater getMenuInflater() {
        return new GeckoMenuInflater(this);
    }

    public MenuPanel getMenuPanel() {
        if (mMenuPanel == null) {
            onCreatePanelMenu(Window.FEATURE_OPTIONS_PANEL, null);
            invalidateOptionsMenu();
        }
        return mMenuPanel;
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        return onOptionsItemSelected(item);
    }

    @Override
    public boolean onMenuItemLongClick(MenuItem item) {
        return false;
    }

    @Override
    public void openMenu() {
        openOptionsMenu();
    }

    @Override
    public void showMenu(final View menu) {
        // On devices using the custom menu, focus is cleared from the menu when its tapped.
        // Close and then reshow it to avoid these issues. See bug 794581 and bug 968182.
        closeMenu();

        // Post the reshow code back to the UI thread to avoid some optimizations Android
        // has put in place for menus that hide/show themselves quickly. See bug 985400.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mMenuPanel.removeAllViews();
                mMenuPanel.addView(menu);
                openOptionsMenu();
            }
        });
    }

    @Override
    public void closeMenu() {
        closeOptionsMenu();
    }

    @Override
    public View onCreatePanelView(int featureId) {
        if (featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = new MenuPanel(this, null);
            } else {
                // Prepare the panel every time before showing the menu.
                onPreparePanel(featureId, mMenuPanel, mMenu);
            }

            return mMenuPanel;
        }

        return super.onCreatePanelView(featureId);
    }

    @Override
    public boolean onCreatePanelMenu(int featureId, Menu menu) {
        if (featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = (MenuPanel) onCreatePanelView(featureId);
            }

            GeckoMenu gMenu = new GeckoMenu(this, null);
            gMenu.setCallback(this);
            gMenu.setMenuPresenter(this);
            menu = gMenu;
            mMenuPanel.addView(gMenu);

            return onCreateOptionsMenu(menu);
        }

        return super.onCreatePanelMenu(featureId, menu);
    }

    @Override
    public boolean onPreparePanel(int featureId, View view, Menu menu) {
        if (featureId == Window.FEATURE_OPTIONS_PANEL) {
            return onPrepareOptionsMenu(menu);
        }

        return super.onPreparePanel(featureId, view, menu);
    }

    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        // exit full-screen mode whenever the menu is opened
        if (mLayerView != null && mLayerView.isFullScreen()) {
            EventDispatcher.getInstance().dispatch("FullScreen:Exit", null);
        }

        if (featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenu == null) {
                // getMenuPanel() will force the creation of the menu as well
                MenuPanel panel = getMenuPanel();
                onPreparePanel(featureId, panel, mMenu);
            }

            // Scroll custom menu to the top
            if (mMenuPanel != null)
                mMenuPanel.scrollTo(0, 0);

            return true;
        }

        return super.onMenuOpened(featureId, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.quit) {
            // Make sure the Guest Browsing notification goes away when we quit.
            GuestSession.hideNotification(this);

            final SharedPreferences prefs = getSharedPreferencesForProfile();
            final Set<String> clearSet = PrefUtils.getStringSet(
                    prefs, ClearOnShutdownPref.PREF, new HashSet<String>());

            final GeckoBundle clearObj = new GeckoBundle(clearSet.size());
            for (final String clear : clearSet) {
                clearObj.putBoolean(clear, true);
            }

            final GeckoBundle res = new GeckoBundle(2);
            res.putBundle("sanitize", clearObj);

            // If the user wants to clear open tabs, or else has opted out of session
            // restore and does want to clear history, we also want to prevent the current
            // session info from being saved.
            if (clearObj.containsKey("private.data.openTabs")) {
                res.putBoolean("dontSaveSession", true);
            } else if (clearObj.containsKey("private.data.history")) {

                final String sessionRestore =
                        getSessionRestorePreference(getSharedPreferences());
                res.putBoolean("dontSaveSession", "quit".equals(sessionRestore));
            }

            EventDispatcher.getInstance().dispatch("Browser:Quit", res);

            // We don't call shutdown here because this creates a race condition which
            // can cause the clearing of private data to fail. Instead, we shut down the
            // UI only after we're done sanitizing.
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onOptionsMenuClosed(Menu menu) {
        mMenuPanel.removeAllViews();
        mMenuPanel.addView((GeckoMenu) mMenu);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Handle hardware menu key presses separately so that we can show a custom menu in some cases.
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            openOptionsMenu();
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putBoolean(SAVED_STATE_IN_BACKGROUND, isApplicationInBackground());
        outState.putString(SAVED_STATE_PRIVATE_SESSION, mPrivateBrowsingSession);
        outState.putInt(LAST_SELECTED_TAB, mLastSelectedTabId);
        outState.putString(LAST_SESSION_UUID, mLastSessionUUID);
    }

    public void addTab() { }

    public void addPrivateTab() { }

    public void showNormalTabs() { }

    public void showPrivateTabs() { }

    public void hideTabs() { }

    /**
     * Close the tab UI indirectly (not as the result of a direct user
     * action).  This does not force the UI to close; for example in Firefox
     * tablet mode it will remain open unless the user explicitly closes it.
     *
     * @return True if the tab UI was hidden.
     */
    public boolean autoHideTabs() { return false; }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (event.equals("Gecko:Ready")) {
            mGeckoReadyStartupTimer.stop();
            geckoConnected();

            // This method is already running on the background thread, so we
            // know that mHealthRecorder will exist. That doesn't stop us being
            // paranoid.
            // This method is cheap, so don't spawn a new runnable.
            final HealthRecorder rec = mHealthRecorder;
            if (rec != null) {
              rec.recordGeckoStartupTime(mGeckoReadyStartupTimer.getElapsed());
            }

            ((GeckoApplication) getApplicationContext()).onDelayedStartup();

            // Reset the crash loop counter if we remain alive for at least half a minute.
            ThreadUtils.postDelayedToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    getSharedPreferences().edit().putInt(PREFS_CRASHED_COUNT, 0).apply();
                }
            }, STARTUP_PHASE_DURATION_MS);

        } else if ("Accessibility:Ready".equals(event)) {
            GeckoAccessibility.updateAccessibilitySettings(this);

        } else if ("Accessibility:Event".equals(event)) {
            GeckoAccessibility.sendAccessibilityEvent(message);

        } else if ("Bookmark:Insert".equals(event)) {
            final BrowserDB db = BrowserDB.from(getProfile());
            final boolean bookmarkAdded = db.addBookmark(
                    getContentResolver(), message.getString("title"), message.getString("url"));
            final int resId = bookmarkAdded ? R.string.bookmark_added
                                            : R.string.bookmark_already_added;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    SnackbarBuilder.builder(GeckoApp.this)
                            .message(resId)
                            .duration(Snackbar.LENGTH_LONG)
                            .buildAndShow();
                }
            });

        } else if ("Contact:Add".equals(event)) {
            final String email = message.getString("email");
            final String phone = message.getString("phone");
            if (email != null) {
                Uri contactUri = Uri.parse(email);
                Intent i = new Intent(ContactsContract.Intents.SHOW_OR_CREATE_CONTACT, contactUri);
                startActivity(i);
            } else if (phone != null) {
                Uri contactUri = Uri.parse(phone);
                Intent i = new Intent(ContactsContract.Intents.SHOW_OR_CREATE_CONTACT, contactUri);
                startActivity(i);
            } else {
                // something went wrong.
                Log.e(LOGTAG, "Received Contact:Add message with no email nor phone number");
            }

        } else if ("DevToolsAuth:Scan".equals(event)) {
            DevToolsAuthHelper.scan(this, callback);

        } else if ("DOMFullScreen:Start".equals(event)) {
            // Local ref to layerView for thread safety
            LayerView layerView = mLayerView;
            if (layerView != null) {
                layerView.setFullScreenState(message.getBoolean("rootElement")
                        ? FullScreenState.ROOT_ELEMENT : FullScreenState.NON_ROOT_ELEMENT);
            }

        } else if ("DOMFullScreen:Stop".equals(event)) {
            // Local ref to layerView for thread safety
            LayerView layerView = mLayerView;
            if (layerView != null) {
                layerView.setFullScreenState(FullScreenState.NONE);
            }

        } else if ("Image:SetAs".equals(event)) {
            String src = message.getString("url");
            setImageAs(src);

        } else if ("Locale:Set".equals(event)) {
            setLocale(message.getString("locale"));

        } else if ("Permissions:Data".equals(event)) {
            final GeckoBundle[] permissions = message.getBundleArray("permissions");
            showSiteSettingsDialog(permissions);

        } else if ("PrivateBrowsing:Data".equals(event)) {
            mPrivateBrowsingSession = message.getString("session");

        } else if ("RuntimePermissions:Check".equals(event)) {
            final String[] permissions = message.getStringArray("permissions");
            final boolean shouldPrompt = message.getBoolean("shouldPrompt", false);
            if (callback == null || permissions == null) {
                return;
            }

            Permissions.from(this)
                    .withPermissions(permissions)
                    .doNotPromptIf(!shouldPrompt)
                    .andFallback(new Runnable() {
                        @Override
                        public void run() {
                            callback.sendSuccess(false);
                        }
                    })
                    .run(new Runnable() {
                        @Override
                        public void run() {
                            callback.sendSuccess(true);
                        }
                    });

        } else if ("Share:Text".equals(event)) {
            final String text = message.getString("text");
            final Tab tab = Tabs.getInstance().getSelectedTab();
            String title = "";
            if (tab != null) {
                title = tab.getDisplayTitle();
            }
            IntentHelper.openUriExternal(text, "text/plain", "", "", Intent.ACTION_SEND, title, false);

            // Context: Sharing via chrome list (no explicit session is active)
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "text");

        } else if ("Snackbar:Show".equals(event)) {
            SnackbarBuilder.builder(this)
                    .fromEvent(message)
                    .callback(callback)
                    .buildAndShow();

        } else if ("SystemUI:Visibility".equals(event)) {
            if (message.getBoolean("visible", true)) {
                mMainLayout.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
            } else {
                mMainLayout.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LOW_PROFILE);
            }

        } else if ("ToggleChrome:Focus".equals(event)) {
            focusChrome();

        } else if ("ToggleChrome:Hide".equals(event)) {
            toggleChrome(false);

        } else if ("ToggleChrome:Show".equals(event)) {
            toggleChrome(true);

        } else if ("Update:Check".equals(event)) {
            UpdateServiceHelper.checkForUpdate(this);

        } else if ("Update:Download".equals(event)) {
            UpdateServiceHelper.downloadUpdate(this);

        } else if ("Update:Install".equals(event)) {
            UpdateServiceHelper.applyUpdate(this);

        } else if ("PluginHelper:playFlash".equals(event)) {
            final SharedPreferences prefs = getSharedPreferences();
            int count = prefs.getInt(PREFS_FLASH_USAGE, 0);
            prefs.edit().putInt(PREFS_FLASH_USAGE, ++count).apply();

        } else if ("Mma:reader_available".equals(event)) {
            MmaDelegate.track(LOADS_ARTICLES);

        } else if ("Mma:web_save_media".equals(event) || "Mma:web_save_image".equals(event)) {
            MmaDelegate.track(DOWNLOAD_VIDEOS_OR_ANY_OTHER_MEDIA);

        }

    }

    /**
     * To get a presenter which will response for text-selection. In preMarshmallow Android we want
     * to provide different UI action when user select a text. Text-selection class will uses this
     * presenter to trigger UI updating.
     *
     * @return a presenter which handle showing/hiding of action mode UI. return *null* if this
     * activity doesn't handle any text-selection event.
     */
    protected ActionModePresenter getTextSelectPresenter() {
        return null;
    }

    /**
     * @param permissions
     *        Array of JSON objects to represent site permissions.
     *        Example: { type: "offline-app", setting: "Store Offline Data", value: "Allow" }
     */
    private void showSiteSettingsDialog(final GeckoBundle[] permissions) {
        final AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.site_settings_title);

        final ArrayList<HashMap<String, String>> itemList =
                new ArrayList<HashMap<String, String>>();
        for (final GeckoBundle permObj : permissions) {
            final HashMap<String, String> map = new HashMap<String, String>();
            map.put("setting", permObj.getString("setting"));
            map.put("value", permObj.getString("value"));
            itemList.add(map);
        }

        // setMultiChoiceItems doesn't support using an adapter, so we're creating a hack with
        // setSingleChoiceItems and changing the choiceMode below when we create the dialog
        builder.setSingleChoiceItems(new SimpleAdapter(
            GeckoApp.this,
            itemList,
            R.layout.site_setting_item,
            new String[] { "setting", "value" },
            new int[] { R.id.setting, R.id.value }
            ), -1, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int id) { }
            });

        builder.setPositiveButton(R.string.site_settings_clear, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int id) {
                ListView listView = ((AlertDialog) dialog).getListView();
                SparseBooleanArray checkedItemPositions = listView.getCheckedItemPositions();

                // An array of the indices of the permissions we want to clear.
                final ArrayList<Integer> permissionsToClear = new ArrayList<>();
                for (int i = 0; i < checkedItemPositions.size(); i++) {
                    if (checkedItemPositions.valueAt(i)) {
                        permissionsToClear.add(checkedItemPositions.keyAt(i));
                    }
                }

                final GeckoBundle data = new GeckoBundle(1);
                data.putIntArray("permissions", permissionsToClear);
                EventDispatcher.getInstance().dispatch("Permissions:Clear", data);
            }
        });

        builder.setNegativeButton(R.string.site_settings_cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int id) {
                dialog.cancel();
            }
        });

        AlertDialog dialog = builder.create();
        dialog.show();

        final ListView listView = dialog.getListView();
        if (listView != null) {
            listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
        }

        final Button clearButton = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
        clearButton.setEnabled(false);

        dialog.getListView().setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
                if (listView.getCheckedItemCount() == 0) {
                    clearButton.setEnabled(false);
                } else {
                    clearButton.setEnabled(true);
                }
            }
        });
    }

    /* package */ void addFullScreenPluginView(View view) {
        if (mFullScreenPluginView != null) {
            Log.w(LOGTAG, "Already have a fullscreen plugin view");
            return;
        }

        setFullScreen(true);

        view.setWillNotDraw(false);
        if (view instanceof SurfaceView) {
            ((SurfaceView) view).setZOrderOnTop(true);
        }

        mFullScreenPluginContainer = new FullScreenHolder(this);

        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            Gravity.CENTER);
        mFullScreenPluginContainer.addView(view, layoutParams);


        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.addView(mFullScreenPluginContainer, layoutParams);

        mFullScreenPluginView = view;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void addPluginView(final View view) {
        final Activity activity = GeckoActivityMonitor.getInstance().getCurrentActivity();
        if (!(activity instanceof GeckoApp)) {
            return;
        }

        final GeckoApp geckoApp = (GeckoApp) activity;
        if (ThreadUtils.isOnUiThread()) {
            geckoApp.addFullScreenPluginView(view);
        } else {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    geckoApp.addFullScreenPluginView(view);
                }
            });
        }
    }

    /* package */ void removeFullScreenPluginView(View view) {
        if (mFullScreenPluginView == null) {
            Log.w(LOGTAG, "Don't have a fullscreen plugin view");
            return;
        }

        if (mFullScreenPluginView != view) {
            Log.w(LOGTAG, "Passed view is not the current full screen view");
            return;
        }

        mFullScreenPluginContainer.removeView(mFullScreenPluginView);

        // We need do do this on the next iteration in order to avoid
        // a deadlock, see comment below in FullScreenHolder
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mLayerView.showSurface();
            }
        });

        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.removeView(mFullScreenPluginContainer);

        mFullScreenPluginView = null;

        GeckoScreenOrientation.getInstance().unlock();
        setFullScreen(false);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void removePluginView(final View view) {
        final Activity activity = GeckoActivityMonitor.getInstance().getCurrentActivity();
        if (!(activity instanceof GeckoApp)) {
            return;
        }

        final GeckoApp geckoApp = (GeckoApp) activity;
        if (ThreadUtils.isOnUiThread()) {
            geckoApp.removeFullScreenPluginView(view);
        } else {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    geckoApp.removeFullScreenPluginView(view);
                }
            });
        }
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    private static native void onFullScreenPluginHidden(View view);

    private void showSetImageResult(final boolean success, final int message, final String path) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (!success) {
                    SnackbarBuilder.builder(GeckoApp.this)
                            .message(message)
                            .duration(Snackbar.LENGTH_LONG)
                            .buildAndShow();
                    return;
                }

                final Intent intent = new Intent(Intent.ACTION_ATTACH_DATA);
                intent.addCategory(Intent.CATEGORY_DEFAULT);
                intent.setData(Uri.parse(path));

                // Removes the image from storage once the chooser activity ends.
                Intent chooser = Intent.createChooser(intent, getString(message));
                ActivityResultHandler handler = new ActivityResultHandler() {
                    @Override
                    public void onActivityResult (int resultCode, Intent data) {
                        getContentResolver().delete(intent.getData(), null, null);
                    }
                };
                ActivityHandlerHelper.startIntentForActivity(GeckoApp.this, chooser, handler);
            }
        });
    }

    // This method starts downloading an image synchronously and displays the Chooser activity to set the image as wallpaper.
    private void setImageAs(final String aSrc) {
        boolean isDataURI = aSrc.startsWith("data:");
        Bitmap image = null;
        InputStream is = null;
        ByteArrayOutputStream os = null;
        try {
            if (isDataURI) {
                int dataStart = aSrc.indexOf(",");
                byte[] buf = Base64.decode(aSrc.substring(dataStart + 1), Base64.DEFAULT);
                image = BitmapUtils.decodeByteArray(buf);
            } else {
                int byteRead;
                byte[] buf = new byte[4192];
                os = new ByteArrayOutputStream();
                URL url = new URL(aSrc);
                is = url.openStream();

                // Cannot read from same stream twice. Also, InputStream from
                // URL does not support reset. So converting to byte array.

                while ((byteRead = is.read(buf)) != -1) {
                    os.write(buf, 0, byteRead);
                }
                byte[] imgBuffer = os.toByteArray();
                image = BitmapUtils.decodeByteArray(imgBuffer);
            }
            if (image != null) {
                // Some devices don't have a DCIM folder and the Media.insertImage call will fail.
                File dcimDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES);

                if (!dcimDir.mkdirs() && !dcimDir.isDirectory()) {
                    showSetImageResult(/* success */ false, R.string.set_image_path_fail, null);
                    return;
                }
                String path = Media.insertImage(getContentResolver(), image, null, null);
                if (path == null) {
                    showSetImageResult(/* success */ false, R.string.set_image_path_fail, null);
                    return;
                }
                showSetImageResult(/* success */ true, R.string.set_image_chooser_title, path);
            } else {
                showSetImageResult(/* success */ false, R.string.set_image_fail, null);
            }
        } catch (OutOfMemoryError ome) {
            Log.e(LOGTAG, "Out of Memory when converting to byte array", ome);
        } catch (IOException ioe) {
            Log.e(LOGTAG, "I/O Exception while setting wallpaper", ioe);
        } finally {
            if (is != null) {
                try {
                    is.close();
                } catch (IOException ioe) {
                    Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                }
            }
            if (os != null) {
                try {
                    os.close();
                } catch (IOException ioe) {
                    Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                }
            }
        }
    }

    private int getBitmapSampleSize(BitmapFactory.Options options, int idealWidth, int idealHeight) {
        int width = options.outWidth;
        int height = options.outHeight;
        int inSampleSize = 1;
        if (height > idealHeight || width > idealWidth) {
            if (width > height) {
                inSampleSize = Math.round((float)height / idealHeight);
            } else {
                inSampleSize = Math.round((float)width / idealWidth);
            }
        }
        return inSampleSize;
    }

    public void requestRender() {
        mLayerView.requestRender();
    }

    @Override // GeckoView.ContentListener
    public void onTitleChange(final GeckoView view, final String title) {
    }

    @Override // GeckoView.ContentListener
    public void onFullScreen(final GeckoView view, final boolean fullScreen) {
        ThreadUtils.assertOnUiThread();
        ActivityUtils.setFullScreen(this, fullScreen);
    }

    protected void setFullScreen(final boolean fullscreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                onFullScreen(mLayerView, fullscreen);
            }
        });
    }

    /**
     * Check and start the Java profiler if MOZ_PROFILER_STARTUP env var is specified.
     **/
    protected static void earlyStartJavaSampler(SafeIntent intent) {
        String env = intent.getStringExtra("env0");
        for (int i = 1; env != null; i++) {
            if (env.startsWith("MOZ_PROFILER_STARTUP=")) {
                if (!env.endsWith("=")) {
                    GeckoJavaSampler.start(10, 1000);
                    Log.d(LOGTAG, "Profiling Java on startup");
                }
                break;
            }
            env = intent.getStringExtra("env" + i);
        }
    }

    /**
     * Called when the activity is first created.
     *
     * Here we initialize all of our profile settings, Firefox Health Report,
     * and other one-shot constructions.
     **/
    @Override
    public void onCreate(Bundle savedInstanceState) {
        GeckoAppShell.ensureCrashHandling();

        // Enable Android Strict Mode for developers' local builds (the "default" channel).
        if ("default".equals(AppConstants.MOZ_UPDATE_CHANNEL)) {
            enableStrictMode();
        }

        // Mozglue should already be loaded by BrowserApp.onCreate() in Fennec, but in
        // custom tabs it may not be.
        GeckoLoader.loadMozGlue(getApplicationContext());

        if (!HardwareUtils.isSupportedSystem() || !GeckoLoader.neonCompatible()) {
            // This build does not support the Android version of the device: Show an error and finish the app.
            mIsAbortingAppLaunch = true;
            super.onCreate(savedInstanceState);
            showSDKVersionError();
            finish();
            return;
        }

        // The clock starts...now. Better hurry!
        mJavaUiStartupTimer = new Telemetry.UptimeTimer("FENNEC_STARTUP_TIME_JAVAUI");
        mGeckoReadyStartupTimer = new Telemetry.UptimeTimer("FENNEC_STARTUP_TIME_GECKOREADY");

        if (savedInstanceState != null) {
            mLastSelectedTabId = savedInstanceState.getInt(LAST_SELECTED_TAB);
            mLastSessionUUID = savedInstanceState.getString(LAST_SESSION_UUID);
        }

        final SafeIntent intent = new SafeIntent(getIntent());

        earlyStartJavaSampler(intent);

        // GeckoLoader wants to dig some environment variables out of the
        // incoming intent, so pass it in here. GeckoLoader will do its
        // business later and dispose of the reference.
        GeckoLoader.setLastIntent(intent);

        // Workaround for <http://code.google.com/p/android/issues/detail?id=20915>.
        try {
            Class.forName("android.os.AsyncTask");
        } catch (ClassNotFoundException e) { }

        MemoryMonitor.getInstance().init(getApplicationContext());

        // GeckoAppShell is tightly coupled to us, rather than
        // the app context, because various parts of Fennec (e.g.,
        // GeckoScreenOrientation) use GAS to access the Activity in
        // the guise of fetching a Context.
        // When that's fixed, `this` can change to
        // `(GeckoApplication) getApplication()` here.
        GeckoAppShell.setContextGetter(this);
        GeckoAppShell.setScreenOrientationDelegate(this);

        // Tell Stumbler to register a local broadcast listener to listen for preference intents.
        // We do this via intents since we can't easily access Stumbler directly,
        // as it might be compiled outside of Fennec.
        getApplicationContext().sendBroadcast(
                new Intent(INTENT_REGISTER_STUMBLER_LISTENER)
        );

        // Did the OS locale change while we were backgrounded? If so,
        // we need to die so that Gecko will re-init add-ons that touch
        // the UI.
        // This is using a sledgehammer to crack a nut, but it'll do for
        // now.
        // Our OS locale pref will be detected as invalid after the
        // restart, and will be propagated to Gecko accordingly, so there's
        // no need to touch that here.
        if (BrowserLocaleManager.getInstance().systemLocaleDidChange()) {
            Log.i(LOGTAG, "System locale changed. Restarting.");
            finishAndShutdown(/* restart */ true);
            return;
        }

        if (sAlreadyLoaded) {
            // This happens when the GeckoApp activity is destroyed by Android
            // without killing the entire application (see Bug 769269).
            // Now that we've got multiple GeckoApp-based activities, this can
            // also happen if we're not the first activity to run within a session.
            mIsRestoringActivity = true;
            Telemetry.addToHistogram("FENNEC_RESTORING_ACTIVITY", 1);

        } else {
            // We're going to restore the last session and/or open a startup/external tab.
            mIgnoreLastSelectedTab = true;

            final String action = intent.getAction();
            final String args = GeckoApplication.addDefaultGeckoArgs(
                    intent.getStringExtra("args"));

            sAlreadyLoaded = true;
            GeckoThread.initMainProcess(/* profile */ null, args,
                                        /* debugging */ ACTION_DEBUG.equals(action));

            // Speculatively pre-fetch the profile in the background.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    getProfile();
                }
            });

            final String uri = getURIFromIntent(intent);
            if (!TextUtils.isEmpty(uri)) {
                // Start a speculative connection as soon as Gecko loads.
                GeckoThread.speculativeConnect(uri);
            }
        }

        // To prevent races, register startup events before launching the Gecko thread.
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Accessibility:Ready",
            "Gecko:Ready",
            "PluginHelper:playFlash",
            null);

        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Update:Check",
            "Update:Download",
            "Update:Install",
            null);

        GeckoThread.launch();

        Bundle stateBundle = IntentUtils.getBundleExtraSafe(getIntent(), EXTRA_STATE_BUNDLE);
        if (stateBundle != null) {
            // Use the state bundle if it was given as an intent extra. This is
            // only intended to be used internally via Robocop, so a boolean
            // is read from a private shared pref to prevent other apps from
            // injecting states.
            final SharedPreferences prefs = getSharedPreferences();
            if (prefs.getBoolean(PREFS_ALLOW_STATE_BUNDLE, false)) {
                prefs.edit().remove(PREFS_ALLOW_STATE_BUNDLE).apply();
                savedInstanceState = stateBundle;
            }
        } else if (savedInstanceState != null) {
            // Bug 896992 - This intent has already been handled; reset the intent.
            setIntent(new Intent(Intent.ACTION_MAIN));
        }

        super.onCreate(savedInstanceState);

        GeckoScreenOrientation.getInstance().update(getResources().getConfiguration().orientation);

        setContentView(getLayout());

        // Set up Gecko layout.
        mRootLayout = (RelativeLayout) findViewById(R.id.root_layout);
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (RelativeLayout) findViewById(R.id.main_layout);
        mLayerView = (GeckoView) findViewById(R.id.layer_view);

        mLayerView.setChromeUri("chrome://browser/content/browser.xul");
        mLayerView.setContentListener(this);

        getAppEventDispatcher().registerGeckoThreadListener(this,
            "Accessibility:Event",
            "Locale:Set",
            null);

        getAppEventDispatcher().registerBackgroundThreadListener(this,
            "Bookmark:Insert",
            "Image:SetAs",
            null);

        getAppEventDispatcher().registerUiThreadListener(this,
            "Contact:Add",
            "DevToolsAuth:Scan",
            "DOMFullScreen:Start",
            "DOMFullScreen:Stop",
            "Mma:reader_available",
            "Mma:web_save_image",
            "Mma:web_save_media",
            "Permissions:Data",
            "PrivateBrowsing:Data",
            "RuntimePermissions:Check",
            "Share:Text",
            "SystemUI:Visibility",
            "ToggleChrome:Focus",
            "ToggleChrome:Hide",
            "ToggleChrome:Show",
            null);

        Tabs.getInstance().attachToContext(this, mLayerView);
        Tabs.registerOnTabsChangedListener(this);

        // Use global layout state change to kick off additional initialization
        mMainLayout.getViewTreeObserver().addOnGlobalLayoutListener(this);

        if (Versions.preMarshmallow) {
            mTextSelection = new ActionBarTextSelection(this, getTextSelectPresenter());
        } else {
            mTextSelection = new FloatingToolbarTextSelection(this, mLayerView);
        }
        mTextSelection.create();

        final Bundle finalSavedInstanceState = savedInstanceState;
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Determine whether we should restore tabs.
                mLastSessionCrashed = updateCrashedState();
                mShouldRestore = getSessionRestoreState(finalSavedInstanceState);
                if (mShouldRestore && finalSavedInstanceState != null) {
                    boolean wasInBackground =
                            finalSavedInstanceState.getBoolean(SAVED_STATE_IN_BACKGROUND, false);

                    // Don't log OOM-kills if only one activity was destroyed. (For example
                    // from "Don't keep activities" on ICS)
                    if (!wasInBackground && !mIsRestoringActivity) {
                        Telemetry.addToHistogram("FENNEC_WAS_KILLED", 1);
                    }

                    mPrivateBrowsingSession =
                            finalSavedInstanceState.getString(SAVED_STATE_PRIVATE_SESSION);
                }

                // If we are doing a restore, read the session data so we can send it to Gecko later.
                GeckoBundle restoreMessage = null;
                if (!mIsRestoringActivity && mShouldRestore) {
                    final boolean isExternalURL = invokedWithExternalURL(getIntentURI(new SafeIntent(getIntent())));
                    try {
                        // restoreSessionTabs() will create simple tab stubs with the
                        // URL and title for each page, but we also need to restore
                        // session history. restoreSessionTabs() will inject the IDs
                        // of the tab stubs into the JSON data (which holds the session
                        // history). This JSON data is then sent to Gecko so session
                        // history can be restored for each tab.
                        restoreMessage = restoreSessionTabs(isExternalURL, false);
                    } catch (SessionRestoreException e) {
                        // If mShouldRestore was set to false in restoreSessionTabs(), this means
                        // either that we intentionally skipped all tabs read from the session file,
                        // or else that the file was syntactically valid, but didn't contain any
                        // tabs (e.g. because the user cleared history), therefore we don't need
                        // to switch to the backup copy.
                        if (mShouldRestore) {
                            Log.e(LOGTAG, "An error occurred during restore, switching to backup file", e);
                            // To be on the safe side, we will always attempt to restore from the backup
                            // copy if we end up here.
                            // Since we will also hit this situation regularly during first run though,
                            // we'll only report it in telemetry if we failed to restore despite the
                            // file existing, which means it's very probably damaged.
                            if (getProfile().sessionFileExists()) {
                                Telemetry.addToHistogram("FENNEC_SESSIONSTORE_DAMAGED_SESSION_FILE", 1);
                            }
                            try {
                                restoreMessage = restoreSessionTabs(isExternalURL, true);
                                Telemetry.addToHistogram("FENNEC_SESSIONSTORE_RESTORING_FROM_BACKUP", 1);
                            } catch (SessionRestoreException ex) {
                                if (!mShouldRestore) {
                                    // Restoring only "failed" because the backup copy was deliberately empty, too.
                                    Telemetry.addToHistogram("FENNEC_SESSIONSTORE_RESTORING_FROM_BACKUP", 1);
                                } else {
                                    // Restoring the backup failed, too, so do a normal startup.
                                    Log.e(LOGTAG, "An error occurred during restore", ex);
                                    mShouldRestore = false;

                                    if (!getSharedPreferencesForProfile().
                                            getBoolean(PREFS_IS_FIRST_RUN, true)) {
                                        // Except when starting with a fresh profile, we should normally
                                        // always have a session file available, even if it might only
                                        // contain an empty window.
                                        Telemetry.addToHistogram("FENNEC_SESSIONSTORE_ALL_FILES_DAMAGED", 1);
                                    }
                                }
                            }
                        }
                    }
                }

                synchronized (GeckoApp.this) {
                    mSessionRestoreParsingFinished = true;
                    GeckoApp.this.notifyAll();
                }

                // If we are doing a restore, send the parsed session data to Gecko.
                if (!mIsRestoringActivity) {
                    EventDispatcher.getInstance().dispatch("Session:Restore", restoreMessage);
                }

                // Make sure sessionstore.old is either updated or deleted as necessary.
                getProfile().updateSessionFile(mShouldRestore);
            }
        });

        // Perform background initialization.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final SharedPreferences prefs = GeckoApp.this.getSharedPreferences();

                // Wait until now to set this, because we'd rather throw an exception than
                // have a caller of BrowserLocaleManager regress startup.
                final LocaleManager localeManager = BrowserLocaleManager.getInstance();
                localeManager.initialize(getApplicationContext());

                SessionInformation previousSession = SessionInformation.fromSharedPrefs(prefs);
                if (previousSession.wasKilled()) {
                    Telemetry.addToHistogram("FENNEC_WAS_KILLED", 1);
                }

                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoAppShell.PREFS_OOM_EXCEPTION, false);

                // Put a flag to check if we got a normal `onSaveInstanceState`
                // on exit, or if we were suddenly killed (crash or native OOM).
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);

                editor.apply();

                // The lifecycle of mHealthRecorder is "shortly after onCreate"
                // through "onDestroy" -- essentially the same as the lifecycle
                // of the activity itself.
                final String profilePath = getProfile().getDir().getAbsolutePath();
                final EventDispatcher dispatcher = getAppEventDispatcher();

                // This is the locale prior to fixing it up.
                final Locale osLocale = Locale.getDefault();

                // Both of these are Java-format locale strings: "en_US", not "en-US".
                final String osLocaleString = osLocale.toString();
                String appLocaleString = localeManager.getAndApplyPersistedLocale(GeckoApp.this);
                Log.d(LOGTAG, "OS locale is " + osLocaleString + ", app locale is " + appLocaleString);

                if (appLocaleString == null) {
                    appLocaleString = osLocaleString;
                }

                mHealthRecorder = GeckoApp.this.createHealthRecorder(GeckoApp.this,
                                                                     profilePath,
                                                                     dispatcher,
                                                                     osLocaleString,
                                                                     appLocaleString,
                                                                     previousSession);

                final String uiLocale = appLocaleString;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        GeckoApp.this.onLocaleReady(uiLocale);
                    }
                });

                // We use per-profile prefs here, because we're tracking against
                // a Gecko pref. The same applies to the locale switcher!
                BrowserLocaleManager.storeAndNotifyOSLocale(getSharedPreferencesForProfile(), osLocale);
            }
        });

        IntentHelper.init(this);
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mIsAbortingAppLaunch) {
            return;
        }

        mWasFirstTabShownAfterActivityUnhidden = false; // onStart indicates we were hidden.
    }

    @Override
    protected void onStop() {
        super.onStop();
        // Overriding here is not necessary, but we do this so we don't
        // forget to add the abort if we override this method later.
        if (mIsAbortingAppLaunch) {
            return;
        }
    }


    /**
     * Derived classes may call this if they require something to be done *after* they've
     * done their onStop() handling.
     */
    protected void onAfterStop() {
        final SharedPreferences sharedPrefs = getSharedPreferencesForProfile();
        if (sharedPrefs.getBoolean(PREFS_IS_FIRST_RUN, true)) {
            sharedPrefs.edit().putBoolean(PREFS_IS_FIRST_RUN, false).apply();
        }
    }

    /**
     * At this point, the resource system and the rest of the browser are
     * aware of the locale.
     *
     * Now we can display strings!
     *
     * You can think of this as being something like a second phase of onCreate,
     * where you can do string-related operations. Use this in place of embedding
     * strings in view XML.
     *
     * By contrast, onConfigurationChanged does some locale operations, but is in
     * response to device changes.
     */
    @Override
    public void onLocaleReady(final String locale) {
        if (!ThreadUtils.isOnUiThread()) {
            throw new RuntimeException("onLocaleReady must always be called from the UI thread.");
        }

        final Locale loc = Locales.parseLocaleCode(locale);
        if (loc.equals(mLastLocale)) {
            Log.d(LOGTAG, "New locale same as old; onLocaleReady has nothing to do.");
        }
        BrowserLocaleManager.getInstance().updateConfiguration(GeckoApp.this, loc);
        ViewUtil.setLayoutDirection(getWindow().getDecorView(), loc);
        refreshChrome();

        // The URL bar hint needs to be populated.
        TextView urlBar = (TextView) findViewById(R.id.url_bar_title);
        if (urlBar != null) {
            final String hint = getResources().getString(R.string.url_bar_default_text);
            urlBar.setHint(hint);
        } else {
            Log.d(LOGTAG, "No URL bar in GeckoApp. Not loading localized hint string.");
        }

        mLastLocale = loc;

        // Allow onConfigurationChanged to take care of the rest.
        // We don't call this.onConfigurationChanged, because (a) that does
        // work that's unnecessary after this locale action, and (b) it can
        // cause a loop! See Bug 1011008, Comment 12.
        super.onConfigurationChanged(getResources().getConfiguration());
    }

    protected void initializeChrome() {
        mDoorHangerPopup = new DoorHangerPopup(this);
        mDoorHangerPopup.setOnVisibilityChangeListener(this);
        mFormAssistPopup = (FormAssistPopup) findViewById(R.id.form_assist_popup);
    }

    @Override
    public void onDoorHangerShow() {
        final View overlay = getDoorhangerOverlay();
        if (overlay != null) {
            final Animator alphaAnimator = ObjectAnimator.ofFloat(overlay, "alpha", 1);
            alphaAnimator.setDuration(250);

            alphaAnimator.start();
        }
    }

    @Override
    public void onDoorHangerHide() {
        final View overlay = getDoorhangerOverlay();
        if (overlay != null) {
            final Animator alphaAnimator = ObjectAnimator.ofFloat(overlay, "alpha", 0);
            alphaAnimator.setDuration(200);

            alphaAnimator.start();
        }
    }

    /**
     * Loads the initial tab at Fennec startup. If we don't restore tabs, this
     * tab will be about:home, or the homepage if the user has set one.
     * If we've temporarily disabled restoring to break out of a crash loop, we'll show
     * the Recent Tabs folder of the Combined History panel, so the user can manually
     * restore tabs as needed.
     * If we restore tabs, we don't need to create a new tab, unless launch intent specify action
     * to be #android.Intent.ACTION_VIEW, which is launched from widget to create a new tab.
     */
    protected void loadStartupTab(final int flags, String action) {
        if (!mShouldRestore || Intent.ACTION_VIEW.equals(action)) {
            if (mLastSessionCrashed) {
                // The Recent Tabs panel no longer exists, but BrowserApp will redirect us
                // to the Recent Tabs folder of the Combined History panel.
                Tabs.getInstance().loadUrl(AboutPages.getURLForBuiltinPanelType(PanelType.DEPRECATED_RECENT_TABS), flags);
            } else {
                Tabs.getInstance().loadUrl(Tabs.getHomepageForStartupTab(this), flags);
            }
        }
    }

    /**
     * Loads the initial tab at Fennec startup. This tab will load with the given
     * external URL. If that URL is invalid, a startup tab will be loaded.
     *
     * @param url    External URL to load.
     * @param intent External intent whose extras modify the request
     * @param flags  Flags used to load the load
     */
    protected void loadStartupTab(final String url, final SafeIntent intent, final int flags) {
        // Invalid url
        if (url == null) {
            loadStartupTab(flags, intent.getAction());
            return;
        }

        final Tab newTab = Tabs.getInstance().loadUrlWithIntentExtras(url, intent, flags);
        if (ThreadUtils.isOnUiThread()) {
            onTabOpenFromIntent(newTab);
        } else {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    onTabOpenFromIntent(newTab);
                }
            });
        }
    }

    protected String getIntentURI(SafeIntent intent) {
        final String passedUri;
        final String uri = getURIFromIntent(intent);

        if (!TextUtils.isEmpty(uri)) {
            passedUri = uri;
        } else {
            passedUri = null;
        }
        return passedUri;
    }

    private boolean invokedWithExternalURL(String uri) {
        return uri != null && !AboutPages.isAboutHome(uri);
    }

    protected int getNewTabFlags() {
        final boolean isFirstTab = !mWasFirstTabShownAfterActivityUnhidden;

        final SafeIntent intent = new SafeIntent(getIntent());
        final String action = intent.getAction();

        int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED | Tabs.LOADURL_EXTERNAL;
        if (ACTION_HOMESCREEN_SHORTCUT.equals(action)) {
            flags |= Tabs.LOADURL_PINNED;
        }
        if (isFirstTab) {
            flags |= Tabs.LOADURL_FIRST_AFTER_ACTIVITY_UNHIDDEN;
        }

        return flags;
    }

    private void initialize() {
        mInitialized = true;

        mWasFirstTabShownAfterActivityUnhidden = true; // Reset since we'll be loading a tab.

        final SafeIntent intent = new SafeIntent(getIntent());
        final String action = intent.getAction();

        final String passedUri = getIntentURI(intent);

        final boolean isExternalURL = invokedWithExternalURL(passedUri);

        // Start migrating as early as possible, can do this in
        // parallel with Gecko load.
        checkMigrateProfile();

        initializeChrome();

        // We need to wait here because mShouldRestore can revert back to
        // false if a parsing error occurs and the startup tab we load
        // depends on whether we restore tabs or not.
        synchronized (this) {
            while (!mSessionRestoreParsingFinished) {
                try {
                    wait();
                } catch (final InterruptedException e) {
                    // Ignore and wait again.
                }
            }
        }

        if (mIsRestoringActivity && hasGeckoTab(intent)) {
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
            handleSelectTabIntent(intent);
        // External URLs should always be loaded regardless of whether Gecko is
        // already running.
        } else if (isExternalURL) {
            // Restore tabs before opening an external URL so that the new tab
            // is animated properly.
            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);
            processActionViewIntent(new Runnable() {
                @Override
                public void run() {
                    final int flags = getNewTabFlags();
                    loadStartupTab(passedUri, intent, flags);
                }
            });
        } else {
            if (!mIsRestoringActivity) {
                loadStartupTab(Tabs.LOADURL_NEW_TAB, action);
            }

            Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);

            processTabQueue();
        }

        recordStartupActionTelemetry(passedUri, action);

        // Check if launched from data reporting notification.
        if (ACTION_LAUNCH_SETTINGS.equals(action)) {
            Intent settingsIntent = new Intent(GeckoApp.this, GeckoPreferences.class);
            // Copy extras.
            settingsIntent.putExtras(intent.getUnsafe());
            startActivity(settingsIntent);
        }

        mPromptService = new PromptService(this);

        // Trigger the completion of the telemetry timer that wraps activity startup,
        // then grab the duration to give to FHR.
        mJavaUiStartupTimer.stop();
        final long javaDuration = mJavaUiStartupTimer.getElapsed();

        ThreadUtils.getBackgroundHandler().postDelayed(new Runnable() {
            @Override
            public void run() {
                final HealthRecorder rec = mHealthRecorder;
                if (rec != null) {
                    rec.recordJavaStartupTime(javaDuration);
                }
            }
        }, 50);

        final int updateServiceDelay = 30 * 1000;
        ThreadUtils.getBackgroundHandler().postDelayed(new Runnable() {
            @Override
            public void run() {
                UpdateServiceHelper.registerForUpdates(GeckoAppShell.getApplicationContext());
            }
        }, updateServiceDelay);

        if (mIsRestoringActivity) {
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null) {
                Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
            }

            if (GeckoThread.isRunning()) {
                geckoConnected();
            }
        }
    }

    protected void onTabOpenFromIntent(Tab tab) { }

    protected void onTabSelectFromIntent(Tab tab) { }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    @Override
    public void onGlobalLayout() {
        if (Versions.preJB) {
            mMainLayout.getViewTreeObserver().removeGlobalOnLayoutListener(this);
        } else {
            mMainLayout.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        }
        if (!mInitialized) {
            initialize();
        }
    }

    protected void processActionViewIntent(final Runnable openTabsRunnable) {
        // We need to ensure that if we receive a VIEW action and there are tabs queued then the
        // site loaded from the intent is on top (last loaded) and selected with all other tabs
        // being opened behind it. We process the tab queue first and request a callback from the JS - the
        // listener will open the url from the intent as normal when the tab queue has been processed.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (TabQueueHelper.TAB_QUEUE_ENABLED && TabQueueHelper.shouldOpenTabQueueUrls(GeckoApp.this)) {

                    getAppEventDispatcher().registerUiThreadListener(new BundleEventListener() {
                        @Override
                        public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
                            if ("Tabs:TabsOpened".equals(event)) {
                                getAppEventDispatcher().unregisterUiThreadListener(this, "Tabs:TabsOpened");
                                openTabsRunnable.run();
                            }
                        }
                    }, "Tabs:TabsOpened");
                    TabQueueHelper.openQueuedUrls(GeckoApp.this, getProfile(), TabQueueHelper.FILE_NAME, true);
                } else {
                    openTabsRunnable.run();
                }
            }
        });
    }

    @WorkerThread
    private GeckoBundle restoreSessionTabs(final boolean isExternalURL, boolean useBackup)
            throws SessionRestoreException {
        String sessionString = getProfile().readSessionFile(useBackup);
        if (sessionString == null) {
            throw new SessionRestoreException("Could not read from session file");
        }

        // If we are doing an OOM restore, parse the session data and
        // stub the restored tabs immediately. This allows the UI to be
        // updated before Gecko has restored.
        final JSONArray tabs = new JSONArray();
        final JSONObject windowObject = new JSONObject();
        final boolean sessionDataValid;

        LastSessionParser parser = new LastSessionParser(tabs, windowObject, isExternalURL);

        if (mPrivateBrowsingSession == null) {
            sessionDataValid = parser.parse(sessionString);
        } else {
            sessionDataValid = parser.parse(sessionString, mPrivateBrowsingSession);
        }

        if (tabs.length() > 0) {
            try {
                // Update all parent tab IDs ...
                parser.updateParentId(tabs);
                windowObject.put("tabs", tabs);
                // ... and for recently closed tabs as well (if we've got any).
                final JSONArray closedTabs = windowObject.optJSONArray("closedTabs");
                parser.updateParentId(closedTabs);
                windowObject.putOpt("closedTabs", closedTabs);

                if (isExternalURL) {
                    // Pass on the tab we would have selected if we weren't going to open an
                    // external URL later on.
                    windowObject.put("selectedTabId", parser.getStoredSelectedTabId());
                }
                sessionString = new JSONObject().put(
                        "windows", new JSONArray().put(windowObject)).toString();
            } catch (final JSONException e) {
                throw new SessionRestoreException(e);
            }
        } else {
            if (parser.allTabsSkipped() || sessionDataValid) {
                // If we intentionally skipped all tabs we've read from the session file, we
                // set mShouldRestore back to false at this point already, so the calling code
                // can infer that the exception wasn't due to a damaged session store file.
                // The same applies if the session file was syntactically valid and
                // simply didn't contain any tabs.
                mShouldRestore = false;
            }
            throw new SessionRestoreException("No tabs could be read from session file");
        }

        if (saveSelectedStartupTab()) {
            // This activity is something other than our normal tabbed browsing interface and is
            // going to overwrite our tab selection. Therefore we should stash it away for later, so
            // e.g. BrowserApp can display the correct tab if starting up later during this session.
            SharedPreferences.Editor prefs = getSharedPreferencesForProfile().edit();
            prefs.putInt(STARTUP_SELECTED_TAB, parser.getStoredSelectedTabId());
            prefs.putString(STARTUP_SESSION_UUID, GeckoApplication.getSessionUUID());
            prefs.apply();
        }

        final GeckoBundle restoreData = new GeckoBundle(1);
        restoreData.putString("sessionString", sessionString);
        return restoreData;
    }

    /**
     * Activities that don't implement a normal tabbed browsing UI and overwrite the tab selection
     * made by session restoring should probably override this and return true.
     */
    protected boolean saveSelectedStartupTab() {
        return false;
    }

    @RobocopTarget
    public @NonNull EventDispatcher getAppEventDispatcher() {
        if (mLayerView == null) {
            throw new IllegalStateException("Must not call getAppEventDispatcher() until after onCreate()");
        }

        return mLayerView.getEventDispatcher();
    }

    protected static GeckoProfile getProfile() {
        return GeckoThread.getActiveProfile();
    }

    /**
     * Check whether we've crashed during the last browsing session.
     *
     * @return True if the crash reporter ran after the last session.
     */
    protected boolean updateCrashedState() {
        try {
            File crashFlag = new File(GeckoProfileDirectories.getMozillaDirectory(this), "CRASHED");
            if (crashFlag.exists() && crashFlag.delete()) {
                // Set the flag that indicates we were stopped as expected, as
                // the crash reporter has run, so it is not a silent OOM crash.
                getSharedPreferences().edit().putBoolean(PREFS_WAS_STOPPED, true).apply();
                return true;
            }
        } catch (NoMozillaDirectoryException e) {
            // If we can't access the Mozilla directory, we're in trouble anyway.
            Log.e(LOGTAG, "Cannot read crash flag: ", e);
        }
        return false;
    }

    /**
     * Determine whether the session should be restored.
     *
     * @param savedInstanceState Saved instance state given to the activity
     * @return                   Whether to restore
     */
    protected boolean getSessionRestoreState(Bundle savedInstanceState) {
        final SharedPreferences prefs = getSharedPreferences();
        boolean shouldRestore = false;

        final int versionCode = getVersionCode();
        if (getSessionRestoreResumeOnce(prefs)) {
            shouldRestore = true;
        } else if (mLastSessionCrashed) {
            if (incrementCrashCount(prefs) <= getSessionStoreMaxCrashResumes(prefs) &&
                    getSessionRestoreAfterCrashPreference(prefs)) {
                shouldRestore = true;
            } else {
                shouldRestore = false;
            }
        } else if (prefs.getInt(PREFS_VERSION_CODE, 0) != versionCode) {
            // If the version has changed, the user has done an upgrade, so restore
            // previous tabs.
            prefs.edit().putInt(PREFS_VERSION_CODE, versionCode).apply();
            shouldRestore = true;
        } else if (savedInstanceState != null ||
                   getSessionRestorePreference(prefs).equals("always") ||
                   getRestartFromIntent()) {
            // We're coming back from a background kill by the OS, the user
            // has chosen to always restore, or we restarted.
            shouldRestore = true;
        }

        return shouldRestore;
    }

    private boolean getSessionRestoreResumeOnce(SharedPreferences prefs) {
        boolean resumeOnce = prefs.getBoolean(GeckoPreferences.PREFS_RESTORE_SESSION_ONCE, false);
        if (resumeOnce) {
            prefs.edit().putBoolean(GeckoPreferences.PREFS_RESTORE_SESSION_ONCE, false).apply();
        }
        return resumeOnce;
    }

    private int incrementCrashCount(SharedPreferences prefs) {
        final int crashCount = getSuccessiveCrashesCount(prefs) + 1;
        prefs.edit().putInt(PREFS_CRASHED_COUNT, crashCount).apply();
        return crashCount;
    }

    private int getSuccessiveCrashesCount(SharedPreferences prefs) {
        return prefs.getInt(PREFS_CRASHED_COUNT, 0);
    }

    private int getSessionStoreMaxCrashResumes(SharedPreferences prefs) {
        return prefs.getInt(GeckoPreferences.PREFS_RESTORE_SESSION_MAX_CRASH_RESUMES, 1);
    }

    private boolean getSessionRestoreAfterCrashPreference(SharedPreferences prefs) {
        return prefs.getBoolean(GeckoPreferences.PREFS_RESTORE_SESSION_FROM_CRASH, true);
    }

    private String getSessionRestorePreference(SharedPreferences prefs) {
        return prefs.getString(GeckoPreferences.PREFS_RESTORE_SESSION, "always");
    }

    private boolean getRestartFromIntent() {
        return IntentUtils.getBooleanExtraSafe(getIntent(), "didRestart", false);
    }

    /**
     * Enable Android StrictMode checks (for supported OS versions).
     * http://developer.android.com/reference/android/os/StrictMode.html
     */
    private void enableStrictMode() {
        Log.d(LOGTAG, "Enabling Android StrictMode");

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                  .detectAll()
                                  .penaltyLog()
                                  .build());

        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                               .detectAll()
                               .penaltyLog()
                               .build());
    }

    @Override
    protected void onNewIntent(Intent externalIntent) {
        final SafeIntent intent = new SafeIntent(externalIntent);
        final String action = intent.getAction();

        final boolean isFirstTab = !mWasFirstTabShownAfterActivityUnhidden;
        mWasFirstTabShownAfterActivityUnhidden = true; // Reset since we'll be loading a tab.
        if (!Intent.ACTION_MAIN.equals(action)) {
            mIgnoreLastSelectedTab = true;
        }

        // if we were previously OOM killed, we can end up here when launching
        // from external shortcuts, so set this as the intent for initialization
        if (!mInitialized) {
            setIntent(externalIntent);
            return;
        }

        final String uri = getURIFromIntent(intent);
        final String passedUri;
        if (!TextUtils.isEmpty(uri)) {
            passedUri = uri;
        } else {
            passedUri = null;
        }

        if (hasGeckoTab(intent)) {
            // This also covers ACTION_SWITCH_TAB.
            handleSelectTabIntent(intent);
        } else if (ACTION_LOAD.equals(action)) {
            Tabs.getInstance().loadUrl(intent.getDataString());
        } else if (Intent.ACTION_VIEW.equals(action)) {
            processActionViewIntent(new Runnable() {
                @Override
                public void run() {
                    final String url = intent.getDataString();
                    int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED | Tabs.LOADURL_EXTERNAL;
                    if (isFirstTab) {
                        flags |= Tabs.LOADURL_FIRST_AFTER_ACTIVITY_UNHIDDEN;
                    }
                    Tabs.getInstance().loadUrlWithIntentExtras(url, intent, flags);
                }
            });
        } else if (ACTION_HOMESCREEN_SHORTCUT.equals(action)) {
            mLayerView.loadUri(uri, GeckoView.LOAD_SWITCH_TAB);
        } else if (Intent.ACTION_SEARCH.equals(action)) {
            mLayerView.loadUri(uri, GeckoView.LOAD_NEW_TAB);
        } else if (NotificationHelper.HELPER_BROADCAST_ACTION.equals(action)) {
            NotificationHelper.getInstance(getApplicationContext()).handleNotificationIntent(intent);
        } else if (ACTION_LAUNCH_SETTINGS.equals(action)) {
            // Check if launched from data reporting notification.
            Intent settingsIntent = new Intent(GeckoApp.this, GeckoPreferences.class);
            // Copy extras.
            settingsIntent.putExtras(intent.getUnsafe());
            startActivity(settingsIntent);
        }

        recordStartupActionTelemetry(passedUri, action);
    }

    /**
     * Check whether an intent with tab switch extras refers to a tab that
     * is actually existing at the moment.
     *
     * @param intent The intent to be checked.
     * @return True if the tab specified in the intent is existing in our Tabs list.
     */
    protected boolean hasGeckoTab(SafeIntent intent) {
        final int tabId = intent.getIntExtra(INTENT_EXTRA_TAB_ID, INVALID_TAB_ID);
        final String intentSessionUUID = intent.getStringExtra(INTENT_EXTRA_SESSION_UUID);
        final Tab tabToCheck = Tabs.getInstance().getTab(tabId);

        // We only care about comparing session UUIDs if one was specified in the intent.
        // Otherwise, we just try matching the tab ID with one of our open tabs.
        return tabToCheck != null && (!intent.hasExtra(INTENT_EXTRA_SESSION_UUID) ||
                GeckoApplication.getSessionUUID().equals(intentSessionUUID));
    }

    protected void handleSelectTabIntent(SafeIntent intent) {
        final int tabId = intent.getIntExtra(INTENT_EXTRA_TAB_ID, INVALID_TAB_ID);
        final Tab selectedTab = Tabs.getInstance().selectTab(tabId);
        onTabSelectFromIntent(selectedTab);
    }

    /**
     * Handles getting a URI from an intent in a way that is backwards-
     * compatible with our previous implementations.
     */
    protected String getURIFromIntent(SafeIntent intent) {
        final String action = intent.getAction();
        if (ACTION_ALERT_CALLBACK.equals(action) ||
                NotificationHelper.HELPER_BROADCAST_ACTION.equals(action)) {
            return null;
        }

        return intent.getDataString();
    }

    protected int getOrientation() {
        return GeckoScreenOrientation.getInstance().getAndroidOrientation();
    }

    @WrapForJNI(calledFrom = "gecko")
    public static void launchOrBringToFront() {
        final Activity activity = GeckoActivityMonitor.getInstance().getCurrentActivity();

        // Check that BrowserApp is not the current foreground activity.
        if (activity instanceof BrowserApp && ((GeckoApp) activity).foregrounded) {
            return;
        }

        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                        Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                            AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        GeckoAppShell.getApplicationContext().startActivity(intent);
    }

    @Override
    public void onResume()
    {
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        EventDispatcher.getInstance().registerUiThreadListener(this, "Snackbar:Show");

        if (mIsAbortingAppLaunch) {
            return;
        }

        foregrounded = true;

        GeckoAppShell.setScreenOrientationDelegate(this);

        // If mIgnoreLastSelectedTab is set, we're either the first activity to run, so our startup
        // code will (have) handle(d) tab selection, or else we've received a new intent and want to
        // open and select a new tab as well.
        if (!mIgnoreLastSelectedTab) {
            Tab selectedTab = Tabs.getInstance().getSelectedTab();

            // We need to check if we've selected a different tab while no GeckoApp-based activity
            // was in foreground and catch up with any activity switches that might be needed.
            if (selectedTab != null && !selectedTab.getWasSelectedInForeground()) {
                selectedTab.setWasSelectedInForeground(true);
                if (!selectedTab.matchesActivity(this)) {
                    startActivity(IntentHelper.getTabSwitchIntent(selectedTab));
                }

                // When backing out of the app closes the current tab and therefore selects
                // another tab, we don't switch activities even if the newly selected tab has a
                // different type, because doing so would bring us into the foreground again.
                // As this means that the currently selected tab doesn't match the last active
                // GeckoApp, we need to check mSuppressActivitySwitch here as well.
            } else if (mLastActiveGeckoApp == null || mLastActiveGeckoApp.get() != this ||
                    mSuppressActivitySwitch) {
                restoreLastSelectedTab();
            }
        }
        mSuppressActivitySwitch = false;
        mIgnoreLastSelectedTab = false;

        int newOrientation = getResources().getConfiguration().orientation;
        if (GeckoScreenOrientation.getInstance().update(newOrientation)) {
            refreshChrome();
        }

        // We use two times: a pseudo-unique wall-clock time to identify the
        // current session across power cycles, and the elapsed realtime to
        // track the duration of the session.
        final long now = System.currentTimeMillis();
        final long realTime = android.os.SystemClock.elapsedRealtime();

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Now construct the new session on HealthRecorder's behalf. We do this here
                // so it can benefit from a single near-startup prefs commit.
                SessionInformation currentSession = new SessionInformation(now, realTime);

                SharedPreferences prefs = GeckoApp.this.getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);

                if (!mLastSessionCrashed) {
                    // The last session terminated normally,
                    // so we can reset the count of successive crashes.
                    editor.putInt(GeckoApp.PREFS_CRASHED_COUNT, 0);
                }

                currentSession.recordBegin(editor);
                editor.apply();

                final HealthRecorder rec = mHealthRecorder;
                if (rec != null) {
                    rec.setCurrentSession(currentSession);
                    rec.processDelayed();
                } else {
                    Log.w(LOGTAG, "Can't record session: rec is null.");
                }
            }
        });

        Restrictions.update(this);
    }

    /**
     * Called on activity resume if a different (or no) GeckoApp-based activity was previously
     * active within our application.
     */
    protected void restoreLastSelectedTab() { }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (!mWindowFocusInitialized && hasFocus) {
            mWindowFocusInitialized = true;
            // XXX our editor tests require the GeckoView to have focus to pass, so we have to
            // manually shift focus to the GeckoView. requestFocus apparently doesn't work at
            // this stage of starting up, so we have to unset and reset the focusability.
            mLayerView.setFocusable(false);
            mLayerView.setFocusable(true);
            mLayerView.setFocusableInTouchMode(true);
            getWindow().setBackgroundDrawable(null);
        }
    }

    @Override
    public void onPause()
    {
        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Snackbar:Show");

        if (mIsAbortingAppLaunch) {
            super.onPause();
            return;
        }

        foregrounded = false;

        mLastActiveGeckoApp = new WeakReference<GeckoApp>(this);

        final HealthRecorder rec = mHealthRecorder;
        final Context context = this;

        // In some way it's sad that Android will trigger StrictMode warnings
        // here as the whole point is to save to disk while the activity is not
        // interacting with the user.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs = GeckoApp.this.getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
                if (rec != null) {
                    rec.recordSessionEnd("P", editor);
                }

                // onPause might in fact be called even after a crash, but in that case the
                // crash reporter will record this fact for us and we'll pick it up in onCreate.
                mLastSessionCrashed = false;

                // If we haven't done it before, cleanup any old files in our old temp dir
                if (prefs.getBoolean(GeckoApp.PREFS_CLEANUP_TEMP_FILES, true)) {
                    File tempDir = GeckoLoader.getGREDir(GeckoApp.this);
                    FileUtils.delTree(tempDir, new FileUtils.NameAndAgeFilter(null, ONE_DAY_MS), false);

                    editor.putBoolean(GeckoApp.PREFS_CLEANUP_TEMP_FILES, false);
                }

                editor.apply();
            }
        });

        super.onPause();
    }

    @Override
    public void onRestart() {
        if (mIsAbortingAppLaunch) {
            super.onRestart();
            return;
        }

        // Faster on main thread with an async apply().
        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        try {
            SharedPreferences.Editor editor = GeckoApp.this.getSharedPreferences().edit();
            editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
            editor.apply();
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }

        super.onRestart();
    }

    @Override
    public void onDestroy() {
        if (mIsAbortingAppLaunch) {
            // This build does not support the Android version of the device:
            // We did not initialize anything, so skip cleaning up.
            super.onDestroy();
            return;
        }

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Accessibility:Ready",
            "Gecko:Ready",
            "PluginHelper:playFlash",
            null);

        EventDispatcher.getInstance().unregisterUiThreadListener(this,
            "Update:Check",
            "Update:Download",
            "Update:Install",
            null);

        getAppEventDispatcher().unregisterGeckoThreadListener(this,
            "Accessibility:Event",
            "Locale:Set",
            null);

        getAppEventDispatcher().unregisterBackgroundThreadListener(this,
            "Bookmark:Insert",
            "Image:SetAs",
            null);

        getAppEventDispatcher().unregisterUiThreadListener(this,
            "Contact:Add",
            "DevToolsAuth:Scan",
            "DOMFullScreen:Start",
            "DOMFullScreen:Stop",
            "Mma:reader_available",
            "Mma:web_save_image",
            "Mma:web_save_media",
            "Permissions:Data",
            "PrivateBrowsing:Data",
            "RuntimePermissions:Check",
            "Share:Text",
            "SystemUI:Visibility",
            "ToggleChrome:Focus",
            "ToggleChrome:Hide",
            "ToggleChrome:Show",
            null);

        if (mPromptService != null) {
            mPromptService.destroy();
            mPromptService = null;
        }

        final HealthRecorder rec = mHealthRecorder;
        mHealthRecorder = null;
        if (rec != null && rec.isEnabled()) {
            // Closing a HealthRecorder could incur a write.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    rec.close(GeckoApp.this);
                }
            });
        }

        super.onDestroy();

        Tabs.unregisterOnTabsChangedListener(this);

        if (mShutdownOnDestroy) {
            GeckoApplication.shutdown(!mRestartOnShutdown ? null : new Intent(
                    Intent.ACTION_MAIN, /* uri */ null, getApplicationContext(), getClass()));
        }
    }

    public void showSDKVersionError() {
        final String message = getString(R.string.unsupported_sdk_version,
                HardwareUtils.getRealAbi(), Integer.toString(Build.VERSION.SDK_INT));
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    // Get a temporary directory, may return null
    public static File getTempDirectory(@NonNull Context context) {
        return context.getApplicationContext().getExternalFilesDir("temp");
    }

    // Delete any files in our temporary directory
    public static void deleteTempFiles(Context context) {
        File dir = getTempDirectory(context);
        if (dir == null)
            return;
        File[] files = dir.listFiles();
        if (files == null)
            return;
        for (File file : files) {
            file.delete();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        Log.d(LOGTAG, "onConfigurationChanged: " + newConfig.locale);

        final LocaleManager localeManager = BrowserLocaleManager.getInstance();
        final Locale changed = localeManager.onSystemConfigurationChanged(this, getResources(), newConfig, mLastLocale);
        if (changed != null) {
            onLocaleChanged(Locales.getLanguageTag(changed));
        }

        // onConfigurationChanged is not called for 180 degree orientation changes,
        // we will miss such rotations and the screen orientation will not be
        // updated.
        if (GeckoScreenOrientation.getInstance().update(newConfig.orientation)) {
            if (mFormAssistPopup != null)
                mFormAssistPopup.hide();
            refreshChrome();
        }
        super.onConfigurationChanged(newConfig);
    }

    public String getContentProcessName() {
        return AppConstants.MOZ_CHILD_PROCESS_NAME;
    }

    public void addEnvToIntent(Intent intent) {
        Map<String, String> envMap = System.getenv();
        Set<Map.Entry<String, String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String, String>> envIter = envSet.iterator();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String, String> entry = envIter.next();
            intent.putExtra("env" + c, entry.getKey() + "="
                            + entry.getValue());
            c++;
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    protected void finishAndShutdown(final boolean restart) {
        ThreadUtils.assertOnUiThread();

        mShutdownOnDestroy = true;
        mRestartOnShutdown = restart;

        // Shut down the activity and then Gecko.
        if (!isFinishing() && (Versions.preJBMR1 || !isDestroyed())) {
            finish();
        }
    }

    private void checkMigrateProfile() {
        final File profileDir = getProfile().getDir();

        if (profileDir != null) {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    Handler handler = new Handler();
                    handler.postDelayed(new DeferredCleanupTask(), CLEANUP_DEFERRAL_SECONDS * 1000);
                }
            });
        }
    }

    private static class DeferredCleanupTask implements Runnable {
        // The cleanup-version setting is recorded to avoid repeating the same
        // tasks on subsequent startups; CURRENT_CLEANUP_VERSION may be updated
        // if we need to do additional cleanup for future Gecko versions.

        private static final String CLEANUP_VERSION = "cleanup-version";
        private static final int CURRENT_CLEANUP_VERSION = 1;

        @Override
        public void run() {
            final Context context = GeckoAppShell.getApplicationContext();
            long cleanupVersion = GeckoSharedPrefs.forApp(context).getInt(CLEANUP_VERSION, 0);

            if (cleanupVersion < 1) {
                // Reduce device storage footprint by removing .ttf files from
                // the res/fonts directory: we no longer need to copy our
                // bundled fonts out of the APK in order to use them.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=878674.
                File dir = new File("res/fonts");
                if (dir.exists() && dir.isDirectory()) {
                    for (File file : dir.listFiles()) {
                        if (file.isFile() && file.getName().endsWith(".ttf")) {
                            file.delete();
                        }
                    }
                    if (!dir.delete()) {
                        Log.w(LOGTAG, "unable to delete res/fonts directory (not empty?)");
                    }
                }
            }

            // Additional cleanup needed for future versions would go here

            if (cleanupVersion != CURRENT_CLEANUP_VERSION) {
                SharedPreferences.Editor editor = GeckoSharedPrefs.forApp(context).edit();
                editor.putInt(CLEANUP_VERSION, CURRENT_CLEANUP_VERSION);
                editor.apply();
            }
        }
    }

    protected void onDone() {
        moveTaskToBack(true);
    }

    @Override
    public void onBackPressed() {
        if (getSupportFragmentManager().getBackStackEntryCount() > 0) {
            super.onBackPressed();
            return;
        }

        if (autoHideTabs()) {
            return;
        }

        if (mDoorHangerPopup != null && mDoorHangerPopup.isShowing()) {
            mDoorHangerPopup.dismiss();
            return;
        }

        if (mFullScreenPluginView != null) {
            onFullScreenPluginHidden(mFullScreenPluginView);
            removeFullScreenPluginView(mFullScreenPluginView);
            return;
        }

        if (mLayerView != null && mLayerView.isFullScreen()) {
            EventDispatcher.getInstance().dispatch("FullScreen:Exit", null);
            return;
        }

        final Tabs tabs = Tabs.getInstance();
        final Tab tab = tabs.getSelectedTab();
        if (tab == null) {
            onDone();
            return;
        }

        // Give Gecko a chance to handle the back press first, then fallback to the Java UI.
        getAppEventDispatcher().dispatch("Browser:OnBackPressed", null, new EventCallback() {
            @Override
            public void sendSuccess(final Object response) {
                if (!((GeckoBundle) response).getBoolean("handled")) {
                    // Default behavior is Gecko didn't prevent.
                    onDefault();
                }
            }

            @Override
            public void sendError(final Object error) {
                // Default behavior is Gecko didn't prevent, via failure.
                onDefault();
            }

            private void onDefault() {
                ThreadUtils.assertOnUiThread();

                if (tab.doBack()) {
                    return;
                }

                if (tab.isExternal()) {
                    onDone();
                    Tab nextSelectedTab = Tabs.getInstance().getNextTab(tab);
                    // Closing the tab will select the next tab. There's no need to unzombify it
                    // if we're really exiting - switching activities is a different matter, though.
                    if (nextSelectedTab != null && nextSelectedTab.getType() == tab.getType()) {
                        final GeckoBundle data = new GeckoBundle(1);
                        data.putInt("nextSelectedTabId", nextSelectedTab.getId());
                        EventDispatcher.getInstance().dispatch("Tab:KeepZombified", data);
                    }
                    mSuppressActivitySwitch = true;
                    tabs.closeTab(tab);
                    return;
                }

                final int parentId = tab.getParentId();
                final Tab parent = tabs.getTab(parentId);
                if (parent != null) {
                    // The back button should always return to the parent (not a sibling).
                    tabs.closeTab(tab, parent);
                    return;
                }

                onDone();
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!ActivityHandlerHelper.handleActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        Permissions.onRequestPermissionsResult(this, permissions, grantResults);
    }

    private void geckoConnected() {
        mLayerView.setOverScrollMode(View.OVER_SCROLL_NEVER);
    }

    public static class MainLayout extends RelativeLayout {
        private TouchEventInterceptor mTouchEventInterceptor;
        private MotionEventInterceptor mMotionEventInterceptor;

        public MainLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
        }

        public void setTouchEventInterceptor(TouchEventInterceptor interceptor) {
            mTouchEventInterceptor = interceptor;
        }

        public void setMotionEventInterceptor(MotionEventInterceptor interceptor) {
            mMotionEventInterceptor = interceptor;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onInterceptTouchEvent(this, event)) {
                return true;
            }
            return super.onInterceptTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onTouch(this, event)) {
                return true;
            }
            return super.onTouchEvent(event);
        }

        @Override
        public boolean onGenericMotionEvent(MotionEvent event) {
            if (mMotionEventInterceptor != null && mMotionEventInterceptor.onInterceptMotionEvent(this, event)) {
                return true;
            }
            return super.onGenericMotionEvent(event);
        }

        @Override
        public void setDrawingCacheEnabled(boolean enabled) {
            // Instead of setting drawing cache in the view itself, we simply
            // enable drawing caching on its children. This is mainly used in
            // animations (see PropertyAnimator)
            super.setChildrenDrawnWithCacheEnabled(enabled);
        }
    }

    private class FullScreenHolder extends FrameLayout {

        public FullScreenHolder(Context ctx) {
            super(ctx);
            setBackgroundColor(0xff000000);
        }

        @Override
        public void addView(View view, int index) {
            /**
             * This normally gets called when Flash adds a separate SurfaceView
             * for the video. It is unhappy if we have the LayerView underneath
             * it for some reason so we need to hide that. Hiding the LayerView causes
             * its surface to be destroyed, which causes a pause composition
             * event to be sent to Gecko. We synchronously wait for that to be
             * processed. Simultaneously, however, Flash is waiting on a mutex so
             * the post() below is an attempt to avoid a deadlock.
             */
            super.addView(view, index);

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    mLayerView.hideSurface();
                }
            });
        }

        /**
         * The methods below are simply copied from what Android WebKit does.
         * It wasn't ever called in my testing, but might as well
         * keep it in case it is for some reason. The methods
         * all return true because we don't want any events
         * leaking out from the fullscreen view.
         */
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyDown(keyCode, event);
            }
            mFullScreenPluginView.onKeyDown(keyCode, event);
            return true;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyUp(keyCode, event);
            }
            mFullScreenPluginView.onKeyUp(keyCode, event);
            return true;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            return true;
        }

        @Override
        public boolean onTrackballEvent(MotionEvent event) {
            mFullScreenPluginView.onTrackballEvent(event);
            return true;
        }
    }

    private int getVersionCode() {
        int versionCode = 0;
        try {
            versionCode = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
        } catch (NameNotFoundException e) {
            Log.wtf(LOGTAG, getPackageName() + " not found", e);
        }
        return versionCode;
    }

    // FHR reason code for a session end prior to a restart for a
    // locale change.
    private static final String SESSION_END_LOCALE_CHANGED = "L";

    /**
     * This exists so that a locale can be applied in two places: when saved
     * in a nested activity, and then again when we get back up to GeckoApp.
     *
     * GeckoApp needs to do a bunch more stuff than, say, GeckoPreferences.
     */
    protected void onLocaleChanged(final String locale) {
        final boolean startNewSession = true;
        final boolean shouldRestart = false;

        // If the HealthRecorder is not yet initialized (unlikely), the locale change won't
        // trigger a session transition and subsequent events will be recorded in an environment
        // with the wrong locale.
        final HealthRecorder rec = mHealthRecorder;
        if (rec != null) {
            rec.onAppLocaleChanged(locale);
            rec.onEnvironmentChanged(startNewSession, SESSION_END_LOCALE_CHANGED);
        }

        final Runnable runnable = new Runnable() {
            @Override
            public void run() {
                if (!ThreadUtils.isOnUiThread()) {
                    ThreadUtils.postToUiThread(this);
                    return;
                }
                if (!shouldRestart) {
                    GeckoApp.this.onLocaleReady(locale);
                } else {
                    finishAndShutdown(/* restart */ true);
                }
            }
        };

        if (!shouldRestart) {
            ThreadUtils.postToUiThread(runnable);
        } else {
            // Do this in the background so that the health recorder has its
            // time to finish.
            ThreadUtils.postToBackgroundThread(runnable);
        }
    }

    /**
     * Use BrowserLocaleManager to change our persisted and current locales,
     * and poke the system to tell it of our changed state.
     */
    protected void setLocale(final String locale) {
        if (locale == null) {
            return;
        }

        final String resultant = BrowserLocaleManager.getInstance().setSelectedLocale(this, locale);
        if (resultant == null) {
            return;
        }

        onLocaleChanged(resultant);
    }

    protected HealthRecorder createHealthRecorder(final Context context,
                                                  final String profilePath,
                                                  final EventDispatcher dispatcher,
                                                  final String osLocale,
                                                  final String appLocale,
                                                  final SessionInformation previousSession) {
        // GeckoApp does not need to record any health information - return a stub.
        return new StubbedHealthRecorder();
    }

    protected void recordStartupActionTelemetry(final String passedURL, final String action) {
    }

    public GeckoView getGeckoView() {
        return mLayerView;
    }

    @Override
    public boolean setRequestedOrientationForCurrentActivity(int requestedActivityInfoOrientation) {
        // We want to support the Screen Orientation API, and it always makes sense to lock the
        // orientation of a browser Activity, so we support locking.
        if (getRequestedOrientation() == requestedActivityInfoOrientation) {
            return false;
        }
        setRequestedOrientation(requestedActivityInfoOrientation);
        return true;
    }
}
