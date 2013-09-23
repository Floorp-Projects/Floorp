/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.favicons.LoadFaviconTask;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.GeckoLayerClient;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.health.BrowserHealthRecorder;
import org.mozilla.gecko.health.BrowserHealthReporter;
import org.mozilla.gecko.home.BrowserSearch;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import org.mozilla.gecko.widget.GeckoActionProvider;
import org.mozilla.gecko.widget.ButtonToast;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.animation.Interpolator;
import android.widget.RelativeLayout;
import android.widget.Toast;

import java.io.File;
import java.io.FileNotFoundException;
import java.net.URLEncoder;
import java.util.EnumSet;
import java.util.Vector;

abstract public class BrowserApp extends GeckoApp
                                 implements TabsPanel.TabsLayoutChangeListener,
                                            PropertyAnimator.PropertyAnimationListener,
                                            View.OnKeyListener,
                                            GeckoLayerClient.OnMetricsChangedListener,
                                            BrowserSearch.OnSearchListener,
                                            BrowserSearch.OnEditSuggestionListener,
                                            HomePager.OnNewTabsListener,
                                            OnUrlOpenListener {
    private static final String LOGTAG = "GeckoBrowserApp";

    private static final String PREF_CHROME_DYNAMICTOOLBAR = "browser.chrome.dynamictoolbar";

    private static final String ABOUT_HOME = "about:home";

    private static final int TABS_ANIMATION_DURATION = 450;

    private static final int READER_ADD_SUCCESS = 0;
    private static final int READER_ADD_FAILED = 1;
    private static final int READER_ADD_DUPLICATE = 2;

    private static final String ADD_SHORTCUT_TOAST = "add_shortcut_toast";
    public static final String GUEST_BROWSING_ARG = "--guest";

    private static final String STATE_ABOUT_HOME_TOP_PADDING = "abouthome_top_padding";
    private static final String STATE_DYNAMIC_TOOLBAR_ENABLED = "dynamic_toolbar";

    private static final String BROWSER_SEARCH_TAG = "browser_search";
    private BrowserSearch mBrowserSearch;
    private View mBrowserSearchContainer;

    public static BrowserToolbar mBrowserToolbar;
    private HomePager mHomePager;
    private View mHomePagerContainer;
    protected Telemetry.Timer mAboutHomeStartupTimer = null;

    private static final int GECKO_TOOLS_MENU = -1;
    private static final int ADDON_MENU_OFFSET = 1000;
    private class MenuItemInfo {
        public int id;
        public String label;
        public String icon;
        public boolean checkable = false;
        public boolean checked = false;
        public boolean enabled = true;
        public boolean visible = true;
        public int parent;
    }

    // The types of guest mdoe dialogs we show
    private static enum GuestModeDialog {
        ENTERING,
        LEAVING
    }

    private Vector<MenuItemInfo> mAddonMenuItemsCache;
    private PropertyAnimator mMainLayoutAnimator;

    private static final Interpolator sTabsInterpolator = new Interpolator() {
        @Override
        public float getInterpolation(float t) {
            t -= 1.0f;
            return t * t * t * t * t + 1.0f;
        }
    };

    private FindInPageBar mFindInPageBar;

    private boolean mAccessibilityEnabled = false;

    // We'll ask for feedback after the user launches the app this many times.
    private static final int FEEDBACK_LAUNCH_COUNT = 15;

    // Whether the dynamic toolbar pref is enabled.
    private boolean mDynamicToolbarEnabled = false;

    // Stored value of the toolbar height, so we know when it's changed.
    private int mToolbarHeight = 0;

    // Stored value of whether the last metrics change allowed for toolbar
    // scrolling.
    private boolean mDynamicToolbarCanScroll = false;

    private Integer mPrefObserverId;

    private SharedPreferencesHelper mSharedPreferencesHelper;

    private OrderedBroadcastHelper mOrderedBroadcastHelper;

    private BrowserHealthReporter mBrowserHealthReporter;

    private SiteIdentityPopup mSiteIdentityPopup;

    public SiteIdentityPopup getSiteIdentityPopup() {
        if (mSiteIdentityPopup == null)
            mSiteIdentityPopup = new SiteIdentityPopup(this);

        return mSiteIdentityPopup;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case LOCATION_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    maybeCancelFaviconLoad(tab);
                }
                // fall through
            case SELECTED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateHomePagerForTab(tab);

                    if (mSiteIdentityPopup != null)
                        mSiteIdentityPopup.dismiss();

                    final TabsPanel.Panel panel = tab.isPrivate()
                                                ? TabsPanel.Panel.PRIVATE_TABS
                                                : TabsPanel.Panel.NORMAL_TABS;
                    // Delay calling showTabs so that it does not modify the mTabsChangedListeners
                    // array while we are still iterating through the array.
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (areTabsShown() && mTabsPanel.getCurrentPanel() != panel)
                                showTabs(panel);
                        }
                    });
                }
                break;
            case START:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();

                    if (isDynamicToolbarEnabled()) {
                        // Show the toolbar.
                        mLayerView.getLayerMarginsAnimator().showMargins(false);
                    }
                }
                break;
            case LOAD_ERROR:
            case STOP:
            case MENU_UPDATED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();
                }
                break;
            case PAGE_SHOW:
                loadFavicon(tab);
                break;
            case LINK_FAVICON:
                // If tab is not loading and the favicon is updated, we
                // want to load the image straight away. If tab is still
                // loading, we only load the favicon once the page's content
                // is fully loaded.
                if (tab.getState() != Tab.STATE_LOADING) {
                    loadFavicon(tab);
                }
                break;
        }
        super.onTabChanged(tab, msg, data);
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        // Global onKey handler. This is called if the focused UI doesn't
        // handle the key event, and before Gecko swallows the events.
        if (event.getAction() != KeyEvent.ACTION_DOWN) {
            return false;
        }

        // Gamepad support only exists in API-level >= 9
        if (Build.VERSION.SDK_INT >= 9 &&
            (event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_BUTTON_Y:
                    // Toggle/focus the address bar on gamepad-y button.
                    if (mBrowserToolbar.isVisible()) {
                        if (isDynamicToolbarEnabled() && !isHomePagerVisible()) {
                            if (mLayerView != null) {
                                mLayerView.getLayerMarginsAnimator().hideMargins(false);
                                mLayerView.requestFocus();
                            }
                        } else {
                            // Just focus the address bar when about:home is visible
                            // or when the dynamic toolbar isn't enabled.
                            mBrowserToolbar.requestFocusFromTouch();
                        }
                    } else {
                        if (mLayerView != null) {
                            mLayerView.getLayerMarginsAnimator().showMargins(false);
                        }
                        mBrowserToolbar.requestFocusFromTouch();
                    }
                    return true;
                case KeyEvent.KEYCODE_BUTTON_L1:
                    // Go back on L1
                    Tabs.getInstance().getSelectedTab().doBack();
                    return true;
                case KeyEvent.KEYCODE_BUTTON_R1:
                    // Go forward on R1
                    Tabs.getInstance().getSelectedTab().doForward();
                    return true;
            }
        }

        // Check if this was a shortcut. Meta keys exists only on 11+.
        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (Build.VERSION.SDK_INT >= 11 && tab != null && event.isCtrlPressed()) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_LEFT_BRACKET:
                    tab.doBack();
                    return true;

                case KeyEvent.KEYCODE_RIGHT_BRACKET:
                    tab.doForward();
                    return true;

                case KeyEvent.KEYCODE_R:
                    tab.doReload();
                    return true;

                case KeyEvent.KEYCODE_PERIOD:
                    tab.doStop();
                    return true;

                case KeyEvent.KEYCODE_T:
                    addTab();
                    return true;

                case KeyEvent.KEYCODE_W:
                    Tabs.getInstance().closeTab(tab);
                    return true;

                case KeyEvent.KEYCODE_F:
                    mFindInPageBar.show();
                return true;
            }
        }

        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mBrowserToolbar.isEditing() && onKey(null, keyCode, event)) {
            return true;
        }

        if (mBrowserToolbar.onKey(keyCode, event)) {
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    void handleReaderListCountRequest() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final int count = BrowserDB.getReadingListCount(getContentResolver());
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:ListCountReturn", Integer.toString(count)));
            }
        });
    }

    void handleReaderAdded(int result, final String title, final String url) {
        if (result != READER_ADD_SUCCESS) {
            if (result == READER_ADD_FAILED) {
                showToast(R.string.reading_list_failed, Toast.LENGTH_SHORT);
            } else if (result == READER_ADD_DUPLICATE) {
                showToast(R.string.reading_list_duplicate, Toast.LENGTH_SHORT);
            }

            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.addReadingListItem(getContentResolver(), title, url);
                showToast(R.string.reading_list_added, Toast.LENGTH_SHORT);

                final int count = BrowserDB.getReadingListCount(getContentResolver());
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:ListCountUpdated", Integer.toString(count)));
            }
        });
    }

    void handleReaderRemoved(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.removeReadingListItemWithURL(getContentResolver(), url);
                showToast(R.string.reading_list_removed, Toast.LENGTH_SHORT);

                final int count = BrowserDB.getReadingListCount(getContentResolver());
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:ListCountUpdated", Integer.toString(count)));
            }
        });
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mAboutHomeStartupTimer = new Telemetry.Timer("FENNEC_STARTUP_TIME_ABOUTHOME");

        String args = getIntent().getStringExtra("args");
        if (args != null && args.contains(GUEST_BROWSING_ARG)) {
            mProfile = GeckoProfile.createGuestProfile(this);
        } else {
            GeckoProfile.maybeCleanupGuestProfile(this);
        }

        super.onCreate(savedInstanceState);

        mBrowserToolbar = (BrowserToolbar) findViewById(R.id.browser_toolbar);

        ((GeckoApp.MainLayout) mMainLayout).setTouchEventInterceptor(new HideTabsTouchListener());
        ((GeckoApp.MainLayout) mMainLayout).setMotionEventInterceptor(new MotionEventInterceptor() {
            @Override
            public boolean onInterceptMotionEvent(View view, MotionEvent event) {
                // If we get a gamepad panning MotionEvent while the focus is not on the layerview,
                // put the focus on the layerview and carry on
                if (mLayerView != null && !mLayerView.hasFocus() && GamepadUtils.isPanningControl(event)) {
                    if (mHomePager == null) {
                        return false;
                    }

                    if (isHomePagerVisible()) {
                        mLayerView.requestFocus();
                    } else {
                        mHomePager.requestFocus();
                    }
                }
                return false;
            }
        });

        mHomePagerContainer = findViewById(R.id.home_pager_container);

        mBrowserSearchContainer = findViewById(R.id.search_container);
        mBrowserSearch = (BrowserSearch) getSupportFragmentManager().findFragmentByTag(BROWSER_SEARCH_TAG);
        if (mBrowserSearch == null) {
            mBrowserSearch = BrowserSearch.newInstance();
            mBrowserSearch.setUserVisibleHint(false);
        }

        mBrowserToolbar.setOnActivateListener(new BrowserToolbar.OnActivateListener() {
            public void onActivate() {
                enterEditingMode();
            }
        });

        mBrowserToolbar.setOnCommitListener(new BrowserToolbar.OnCommitListener() {
            public void onCommit() {
                commitEditingMode();
            }
        });

        mBrowserToolbar.setOnDismissListener(new BrowserToolbar.OnDismissListener() {
            public void onDismiss() {
                dismissEditingMode();
            }
        });

        mBrowserToolbar.setOnFilterListener(new BrowserToolbar.OnFilterListener() {
            public void onFilter(String searchText, AutocompleteHandler handler) {
                filterEditingMode(searchText, handler);
            }
        });

        mBrowserToolbar.setOnStartEditingListener(new BrowserToolbar.OnStartEditingListener() {
            public void onStartEditing() {
                // Temporarily disable doorhanger notifications.
                mDoorHangerPopup.disable();
            }
        });

        mBrowserToolbar.setOnStopEditingListener(new BrowserToolbar.OnStopEditingListener() {
            public void onStopEditing() {
                // Re-enable doorhanger notifications.
                mDoorHangerPopup.enable();
            }
        });

        // Intercept key events for gamepad shortcuts
        mBrowserToolbar.setOnKeyListener(this);

        if (mTabsPanel != null) {
            mTabsPanel.setTabsLayoutChangeListener(this);
            updateSideBarState();
        }

        mFindInPageBar = (FindInPageBar) findViewById(R.id.find_in_page);

        registerEventListener("CharEncoding:Data");
        registerEventListener("CharEncoding:State");
        registerEventListener("Feedback:LastUrl");
        registerEventListener("Feedback:OpenPlayStore");
        registerEventListener("Feedback:MaybeLater");
        registerEventListener("Telemetry:Gather");
        registerEventListener("Settings:Show");
        registerEventListener("Updater:Launch");
        registerEventListener("Reader:GoToReadingList");

        Distribution.init(this, getPackageResourcePath());
        JavaAddonManager.getInstance().init(getApplicationContext());
        mSharedPreferencesHelper = new SharedPreferencesHelper(getApplicationContext());
        mOrderedBroadcastHelper = new OrderedBroadcastHelper(getApplicationContext());
        mBrowserHealthReporter = new BrowserHealthReporter();

        if (AppConstants.MOZ_ANDROID_BEAM && Build.VERSION.SDK_INT >= 14) {
            NfcAdapter nfc = NfcAdapter.getDefaultAdapter(this);
            if (nfc != null) {
                nfc.setNdefPushMessageCallback(new NfcAdapter.CreateNdefMessageCallback() {
                    @Override
                    public NdefMessage createNdefMessage(NfcEvent event) {
                        Tab tab = Tabs.getInstance().getSelectedTab();
                        if (tab == null || tab.isPrivate()) {
                            return null;
                        }
                        return new NdefMessage(new NdefRecord[] { NdefRecord.createUri(tab.getURL()) });
                    }
                }, this);
            }
        }

        if (savedInstanceState != null) {
            mDynamicToolbarEnabled = savedInstanceState.getBoolean(STATE_DYNAMIC_TOOLBAR_ENABLED);
            mHomePagerContainer.setPadding(0, savedInstanceState.getInt(STATE_ABOUT_HOME_TOP_PADDING), 0, 0);
        }

        // Listen to the dynamic toolbar pref
        mPrefObserverId = PrefsHelper.getPref(PREF_CHROME_DYNAMICTOOLBAR, new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (value == mDynamicToolbarEnabled) {
                    return;
                }
                mDynamicToolbarEnabled = value;

                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // If accessibility is enabled, the dynamic toolbar is
                        // forced to be off.
                        if (!mAccessibilityEnabled) {
                            setDynamicToolbarEnabled(mDynamicToolbarEnabled);
                        }
                    }
                });
            }

            @Override
            public boolean isObserver() {
                // We want to be notified of changes to be able to switch mode
                // without restarting.
                return true;
            }
        });
    }

    @Override
    public void onBackPressed() {
        if (getSupportFragmentManager().getBackStackEntryCount() > 0) {
            super.onBackPressed();
            return;
        }

        if (dismissEditingMode()) {
            return;
        }

        if (mSiteIdentityPopup != null && mSiteIdentityPopup.isShowing()) {
            mSiteIdentityPopup.dismiss();
            return;
        }

        super.onBackPressed();
    }

    @Override
    public void onResume() {
        super.onResume();
        unregisterEventListener("Prompt:ShowTop");
    }

    @Override
    public void onPause() {
        super.onPause();
        // Register for Prompt:ShowTop so we can foreground this activity even if it's hidden.
        registerEventListener("Prompt:ShowTop");
    }

    private void showBookmarkDialog() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        final Prompt ps = new Prompt(this, new Prompt.PromptCallback() {
            @Override
            public void onPromptFinished(String result) {
                int itemId = -1;
                try {
                  itemId = new JSONObject(result).getInt("button");
                } catch(JSONException ex) {
                    Log.e(LOGTAG, "Exception reading bookmark prompt result", ex);
                }

                if (tab == null)
                    return;

                if (itemId == 0) {
                    new EditBookmarkDialog(BrowserApp.this).show(tab.getURL());
                } else if (itemId == 1) {
                    String url = tab.getURL();
                    String title = tab.getDisplayTitle();
                    Bitmap favicon = tab.getFavicon();
                    if (url != null && title != null) {
                        GeckoAppShell.createShortcut(title, url, url, favicon == null ? null : favicon, "");
                    }
                }
            }
        });

        final Prompt.PromptListItem[] items = new Prompt.PromptListItem[2];
        Resources res = getResources();
        items[0] = new Prompt.PromptListItem(res.getString(R.string.contextmenu_edit_bookmark));
        items[1] = new Prompt.PromptListItem(res.getString(R.string.contextmenu_add_to_launcher));

        ps.show("", "", items, false);
    }

    private void setDynamicToolbarEnabled(boolean enabled) {
        if (enabled) {
            if (mLayerView != null) {
                mLayerView.getLayerClient().setOnMetricsChangedListener(this);
            }
            setToolbarMargin(0);
            mHomePagerContainer.setPadding(0, mBrowserToolbar.getHeight(), 0, 0);
        } else {
            // Immediately show the toolbar when disabling the dynamic
            // toolbar.
            if (mLayerView != null) {
                mLayerView.getLayerClient().setOnMetricsChangedListener(null);
            }
            mHomePagerContainer.setPadding(0, 0, 0, 0);
            if (mBrowserToolbar != null) {
                mBrowserToolbar.scrollTo(0, 0);
            }
        }

        refreshToolbarHeight();
    }

    private boolean isDynamicToolbarEnabled() {
        return mDynamicToolbarEnabled && !mAccessibilityEnabled;
    }

    private boolean isAboutHome(Tab tab) {
        return TextUtils.equals(ABOUT_HOME, tab.getURL());
    }

    @Override
    public boolean onSearchRequested() {
        enterEditingMode();
        return true;
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        final int itemId = item.getItemId();
        if (itemId == R.id.pasteandgo) {
            String text = Clipboard.getText();
            if (!TextUtils.isEmpty(text)) {
                Tabs.getInstance().loadUrl(text);
            }
            return true;
        }

        if (itemId == R.id.site_settings) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Get", null));
            return true;
        }

        if (itemId == R.id.paste) {
            String text = Clipboard.getText();
            if (!TextUtils.isEmpty(text)) {
                enterEditingMode(text);
            }
            return true;
        }

        if (itemId == R.id.share) {
            shareCurrentUrl();
            return true;
        }

        if (itemId == R.id.subscribe) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && tab.getFeedsEnabled()) {
                JSONObject args = new JSONObject();
                try {
                    args.put("tabId", tab.getId());
                } catch (JSONException e) {
                    Log.e(LOGTAG, "error building json arguments");
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Feeds:Subscribe", args.toString()));
            }
            return true;
        }

        if (itemId == R.id.copyurl) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                String url = tab.getURL();
                if (url != null) {
                    Clipboard.setText(url);
                }
            }
            return true;
        }

        if (itemId == R.id.add_to_launcher) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                final String url = tab.getURL();
                final String title = tab.getDisplayTitle();
                if (url == null || title == null) {
                    return true;
                }

                Favicons.loadFavicon(url, tab.getFaviconURL(), 0,
                new OnFaviconLoadedListener() {
                    @Override
                    public void onFaviconLoaded(String url, Bitmap favicon) {
                        GeckoAppShell.createShortcut(title, url, url, favicon == null ? null : favicon, "");
                    }
                });
            }
            return true;
        }

        return false;
    }

    @Override
    public void setAccessibilityEnabled(boolean enabled) {
        if (mAccessibilityEnabled == enabled) {
            return;
        }

        // Disable the dynamic toolbar when accessibility features are enabled,
        // and re-read the preference when they're disabled.
        mAccessibilityEnabled = enabled;
        if (mDynamicToolbarEnabled) {
            setDynamicToolbarEnabled(!enabled);
        }
    }

    @Override
    public void onDestroy() {
        if (mPrefObserverId != null) {
            PrefsHelper.removeObserver(mPrefObserverId);
            mPrefObserverId = null;
        }
        if (mBrowserToolbar != null)
            mBrowserToolbar.onDestroy();

        if (mFindInPageBar != null) {
            mFindInPageBar.onDestroy();
            mFindInPageBar = null;
        }

        if (mSharedPreferencesHelper != null) {
            mSharedPreferencesHelper.uninit();
            mSharedPreferencesHelper = null;
        }

        if (mOrderedBroadcastHelper != null) {
            mOrderedBroadcastHelper.uninit();
            mOrderedBroadcastHelper = null;
        }

        if (mBrowserHealthReporter != null) {
            mBrowserHealthReporter.uninit();
            mBrowserHealthReporter = null;
        }

        unregisterEventListener("CharEncoding:Data");
        unregisterEventListener("CharEncoding:State");
        unregisterEventListener("Feedback:LastUrl");
        unregisterEventListener("Feedback:OpenPlayStore");
        unregisterEventListener("Feedback:MaybeLater");
        unregisterEventListener("Telemetry:Gather");
        unregisterEventListener("Settings:Show");
        unregisterEventListener("Updater:Launch");
        unregisterEventListener("Reader:GoToReadingList");

        if (AppConstants.MOZ_ANDROID_BEAM && Build.VERSION.SDK_INT >= 14) {
            NfcAdapter nfc = NfcAdapter.getDefaultAdapter(this);
            if (nfc != null) {
                // null this out even though the docs say it's not needed,
                // because the source code looks like it will only do this
                // automatically on API 14+
                nfc.setNdefPushMessageCallback(null, this);
            }
        }

        super.onDestroy();
    }

    @Override
    protected void initializeChrome() {
        super.initializeChrome();

        mBrowserToolbar.updateBackButton(false);
        mBrowserToolbar.updateForwardButton(false);

        mDoorHangerPopup.setAnchor(mBrowserToolbar.mFavicon);

        // Listen to margin changes to position the toolbar correctly
        if (isDynamicToolbarEnabled()) {
            refreshToolbarHeight();
            mLayerView.getLayerMarginsAnimator().showMargins(true);
            mLayerView.getLayerClient().setOnMetricsChangedListener(this);
        }

        // Intercept key events for gamepad shortcuts
        mLayerView.setOnKeyListener(this);
    }

    private void shareCurrentUrl() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
          return;

        String url = tab.getURL();
        if (url == null)
            return;

        if (ReaderModeUtils.isAboutReader(url))
            url = ReaderModeUtils.getUrlFromAboutReader(url);

        GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                      Intent.ACTION_SEND, tab.getDisplayTitle());
    }

    @Override
    protected void loadStartupTab(String url) {
        // We aren't showing about:home, so cancel the telemetry timer
        if (url != null || mShouldRestore) {
            mAboutHomeStartupTimer.cancel();
        }

        super.loadStartupTab(url);
    }

    private void setToolbarMargin(int margin) {
        ((RelativeLayout.LayoutParams) mGeckoLayout.getLayoutParams()).topMargin = margin;
        mGeckoLayout.requestLayout();
    }

    @Override
    public void onMetricsChanged(ImmutableViewportMetrics aMetrics) {
        if (isHomePagerVisible() || mBrowserToolbar == null) {
            return;
        }

        // If the page has shrunk so that the toolbar no longer scrolls, make
        // sure the toolbar is visible.
        if (aMetrics.getPageHeight() <= aMetrics.getHeight()) {
            if (mDynamicToolbarCanScroll) {
                mDynamicToolbarCanScroll = false;
                if (!mBrowserToolbar.isVisible()) {
                    ThreadUtils.postToUiThread(new Runnable() {
                        public void run() {
                            mLayerView.getLayerMarginsAnimator().showMargins(false);
                        }
                    });
                }
            }
        } else {
            mDynamicToolbarCanScroll = true;
        }

        final View toolbarLayout = mBrowserToolbar;
        final int marginTop = Math.round(aMetrics.marginTop);
        ThreadUtils.postToUiThread(new Runnable() {
            public void run() {
                toolbarLayout.scrollTo(0, toolbarLayout.getHeight() - marginTop);
            }
        });

        if (mFormAssistPopup != null)
            mFormAssistPopup.onMetricsChanged(aMetrics);
    }

    @Override
    public void onPanZoomStopped() {
        if (!isDynamicToolbarEnabled() || isHomePagerVisible()) {
            return;
        }

        // Make sure the toolbar is fully hidden or fully shown when the user
        // lifts their finger. If the page is shorter than the viewport, the
        // toolbar is always shown.
        ImmutableViewportMetrics metrics = mLayerView.getViewportMetrics();
        if (metrics.getPageHeight() < metrics.getHeight()
              || metrics.marginTop >= mToolbarHeight / 2) {
            mLayerView.getLayerMarginsAnimator().showMargins(false);
        } else {
            mLayerView.getLayerMarginsAnimator().hideMargins(false);
        }
    }

    public void refreshToolbarHeight() {
        int height = 0;
        if (mBrowserToolbar != null) {
            height = mBrowserToolbar.getHeight();
        }

        if (!isDynamicToolbarEnabled() || isHomePagerVisible()) {
            // Use aVisibleHeight here so that when the dynamic toolbar is
            // enabled, the padding will animate with the toolbar becoming
            // visible.
            if (isDynamicToolbarEnabled()) {
                // When the dynamic toolbar is enabled, set the padding on the
                // about:home widget directly - this is to avoid resizing the
                // LayerView, which can cause visible artifacts.
                mHomePagerContainer.setPadding(0, height, 0, 0);
            } else {
                setToolbarMargin(height);
                height = 0;
            }
        } else {
            setToolbarMargin(0);
        }

        if (mLayerView != null && height != mToolbarHeight) {
            mToolbarHeight = height;
            mLayerView.getLayerMarginsAnimator().setMaxMargins(0, height, 0, 0);
            mLayerView.getLayerMarginsAnimator().showMargins(true);
        }
    }

    @Override
    void toggleChrome(final boolean aShow) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (aShow) {
                    mBrowserToolbar.show();
                } else {
                    mBrowserToolbar.hide();
                    if (hasTabsSideBar()) {
                        hideTabs();
                    }
                }
            }
        });

        super.toggleChrome(aShow);
    }

    @Override
    void focusChrome() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mBrowserToolbar.show();
                mBrowserToolbar.requestFocusFromTouch();
            }
        });
    }

    @Override
    public void refreshChrome() {
        invalidateOptionsMenu();
        updateSideBarState();
        mTabsPanel.refresh();
        if (mSiteIdentityPopup != null) {
            mSiteIdentityPopup.dismiss();
        }
    }

    @Override
    public boolean hasTabsSideBar() {
        return (mTabsPanel != null && mTabsPanel.isSideBar());
    }

    private void updateSideBarState() {
        if (mMainLayoutAnimator != null)
            mMainLayoutAnimator.stop();

        boolean isSideBar = (HardwareUtils.isTablet() && mOrientation == Configuration.ORIENTATION_LANDSCAPE);
        final int sidebarWidth = getResources().getDimensionPixelSize(R.dimen.tabs_sidebar_width);

        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) mTabsPanel.getLayoutParams();
        lp.width = (isSideBar ? sidebarWidth : ViewGroup.LayoutParams.FILL_PARENT);
        mTabsPanel.requestLayout();

        final boolean sidebarIsShown = (isSideBar && mTabsPanel.isShown());
        final int mainLayoutScrollX = (sidebarIsShown ? -sidebarWidth : 0);
        mMainLayout.scrollTo(mainLayoutScrollX, 0);

        mTabsPanel.setIsSideBar(isSideBar);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Menu:Add")) {
                MenuItemInfo info = new MenuItemInfo();
                info.label = message.getString("name");
                info.id = message.getInt("id") + ADDON_MENU_OFFSET;
                info.icon = message.optString("icon", null);
                info.checked = message.optBoolean("checked", false);
                info.enabled = message.optBoolean("enabled", true);
                info.visible = message.optBoolean("visible", true);
                info.checkable = message.optBoolean("checkable", false);
                int parent = message.optInt("parent", 0);
                info.parent = parent <= 0 ? parent : parent + ADDON_MENU_OFFSET;
                final MenuItemInfo menuItemInfo = info;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addAddonMenuItem(menuItemInfo);
                    }
                });
            } else if (event.equals("Menu:Remove")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        removeAddonMenuItem(id);
                    }
                });
            } else if (event.equals("Menu:Update")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                final JSONObject options = message.getJSONObject("options");
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        updateAddonMenuItem(id, options);
                    }
                });
            } else if (event.equals("CharEncoding:Data")) {
                final JSONArray charsets = message.getJSONArray("charsets");
                int selected = message.getInt("selected");

                final int len = charsets.length();
                final String[] titleArray = new String[len];
                for (int i = 0; i < len; i++) {
                    JSONObject charset = charsets.getJSONObject(i);
                    titleArray[i] = charset.getString("title");
                }

                final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
                dialogBuilder.setSingleChoiceItems(titleArray, selected, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        try {
                            JSONObject charset = charsets.getJSONObject(which);
                            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("CharEncoding:Set", charset.getString("code")));
                            dialog.dismiss();
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "error parsing json", e);
                        }
                    }
                });
                dialogBuilder.setNegativeButton(R.string.button_cancel, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        dialogBuilder.show();
                    }
                });
            } else if (event.equals("CharEncoding:State")) {
                final boolean visible = message.getString("visible").equals("true");
                GeckoPreferences.setCharEncodingState(visible);
                final Menu menu = mMenu;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (menu != null)
                            menu.findItem(R.id.char_encoding).setVisible(visible);
                    }
                });
            } else if (event.equals("Feedback:OpenPlayStore")) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse("market://details?id=" + getPackageName()));
                startActivity(intent);
            } else if (event.equals("Feedback:MaybeLater")) {
                resetFeedbackLaunchCount();
            } else if (event.equals("Feedback:LastUrl")) {
                getLastUrl();
            } else if (event.equals("Gecko:Ready")) {
                // Handle this message in GeckoApp, but also enable the Settings
                // menuitem, which is specific to BrowserApp.
                super.handleMessage(event, message);
                final Menu menu = mMenu;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (menu != null)
                            menu.findItem(R.id.settings).setEnabled(true);
                    }
                });

                // Display notification for Mozilla data reporting, if data should be collected.
                if (AppConstants.MOZ_DATA_REPORTING) {
                    DataReportingNotification.checkAndNotifyPolicy(GeckoAppShell.getContext());
                }

            } else if (event.equals("Telemetry:Gather")) {
                Telemetry.HistogramAdd("PLACES_PAGES_COUNT", BrowserDB.getCount(getContentResolver(), "history"));
                Telemetry.HistogramAdd("PLACES_BOOKMARKS_COUNT", BrowserDB.getCount(getContentResolver(), "bookmarks"));
                Telemetry.HistogramAdd("FENNEC_FAVICONS_COUNT", BrowserDB.getCount(getContentResolver(), "favicons"));
                Telemetry.HistogramAdd("FENNEC_THUMBNAILS_COUNT", BrowserDB.getCount(getContentResolver(), "thumbnails"));
            } else if (event.equals("Reader:ListCountRequest")) {
                handleReaderListCountRequest();
            } else if (event.equals("Reader:Added")) {
                final int result = message.getInt("result");
                final String title = message.getString("title");
                final String url = message.getString("url");
                handleReaderAdded(result, title, url);
            } else if (event.equals("Reader:Removed")) {
                final String url = message.getString("url");
                handleReaderRemoved(url);
            } else if (event.equals("Reader:Share")) {
                final String title = message.getString("title");
                final String url = message.getString("url");
                GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                              Intent.ACTION_SEND, title);
            } else if (event.equals("Settings:Show")) {
                // null strings return "null" (http://code.google.com/p/android/issues/detail?id=13830)
                String resource = null;
                if (!message.isNull(GeckoPreferences.INTENT_EXTRA_RESOURCES)) {
                    resource = message.getString(GeckoPreferences.INTENT_EXTRA_RESOURCES);
                }
                Intent settingsIntent = new Intent(this, GeckoPreferences.class);
                GeckoPreferences.setResourceToOpen(settingsIntent, resource);
                startActivity(settingsIntent);
            } else if (event.equals("Updater:Launch")) {
                handleUpdaterLaunch();
            } else if (event.equals("Reader:GoToReadingList")) {
                openReadingList();
            } else if (event.equals("Prompt:ShowTop")) {
                // Bring this activity to front so the prompt is visible..
                Intent bringToFrontIntent = new Intent();
                bringToFrontIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.BROWSER_INTENT_CLASS);
                bringToFrontIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
                startActivity(bringToFrontIntent);
            } else {
                super.handleMessage(event, message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    @Override
    public void addTab() {
        Tabs.getInstance().loadUrl("about:home", Tabs.LOADURL_NEW_TAB);
    }

    @Override
    public void addPrivateTab() {
        Tabs.getInstance().loadUrl("about:privatebrowsing", Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
    }

    @Override
    public void showNormalTabs() {
        showTabs(TabsPanel.Panel.NORMAL_TABS);
    }

    @Override
    public void showPrivateTabs() {
        showTabs(TabsPanel.Panel.PRIVATE_TABS);
    }

    @Override
    public void showRemoteTabs() {
        showTabs(TabsPanel.Panel.REMOTE_TABS);
    }

    private void showTabs(TabsPanel.Panel panel) {
        if (Tabs.getInstance().getDisplayCount() == 0)
            return;

        mTabsPanel.show(panel);
    }

    @Override
    public void hideTabs() {
        mTabsPanel.hide();
    }

    @Override
    public boolean autoHideTabs() {
        if (areTabsShown()) {
            hideTabs();
            return true;
        }
        return false;
    }

    @Override
    public boolean areTabsShown() {
        return mTabsPanel.isShown();
    }

    @Override
    public void onTabsLayoutChange(int width, int height) {
        int animationLength = TABS_ANIMATION_DURATION;

        if (mMainLayoutAnimator != null) {
            animationLength = Math.max(1, animationLength - (int)mMainLayoutAnimator.getRemainingTime());
            mMainLayoutAnimator.stop(false);
        }

        if (areTabsShown()) {
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        }

        mMainLayoutAnimator = new PropertyAnimator(animationLength, sTabsInterpolator);
        mMainLayoutAnimator.addPropertyAnimationListener(this);

        if (hasTabsSideBar()) {
            mMainLayoutAnimator.attach(mMainLayout,
                                       PropertyAnimator.Property.SCROLL_X,
                                       -width);
        } else {
            mMainLayoutAnimator.attach(mMainLayout,
                                       PropertyAnimator.Property.SCROLL_Y,
                                       -height);
        }

        mTabsPanel.prepareTabsAnimation(mMainLayoutAnimator);
        mBrowserToolbar.prepareTabsAnimation(mMainLayoutAnimator, areTabsShown());

        // If the tabs layout is animating onto the screen, pin the dynamic
        // toolbar.
        if (mLayerView != null && isDynamicToolbarEnabled()) {
            if (width > 0 && height > 0) {
                mLayerView.getLayerMarginsAnimator().setMarginsPinned(true);
                mLayerView.getLayerMarginsAnimator().showMargins(false);
            } else {
                mLayerView.getLayerMarginsAnimator().setMarginsPinned(false);
            }
        }

        mMainLayoutAnimator.start();
    }

    @Override
    public void onPropertyAnimationStart() {
    }

    @Override
    public void onPropertyAnimationEnd() {
        if (!areTabsShown()) {
            mTabsPanel.setVisibility(View.INVISIBLE);
            mTabsPanel.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        }

        mTabsPanel.finishTabsAnimation();

        mMainLayoutAnimator = null;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mToast.onSaveInstanceState(outState);
        outState.putBoolean(STATE_DYNAMIC_TOOLBAR_ENABLED, mDynamicToolbarEnabled);
        outState.putInt(STATE_ABOUT_HOME_TOP_PADDING, mHomePagerContainer.getPaddingTop());
    }

    /**
     * Attempts to switch to an open tab with the given URL.
     *
     * @return true if we successfully switched to a tab, false otherwise.
     */
    private boolean maybeSwitchToTab(String url, EnumSet<OnUrlOpenListener.Flags> flags) {
        if (!flags.contains(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB)) {
            return false;
        }

        final Tabs tabs = Tabs.getInstance();
        final int tabId = tabs.getTabIdForUrl(url);
        if (tabId < 0) {
            return false;
        }

        // If this tab is already selected, just hide the home pager.
        if (tabs.isSelectedTab(tabs.getTab(tabId))) {
            hideHomePager();
        } else {
            tabs.selectTab(tabId);
        }

        hideBrowserSearch();
        mBrowserToolbar.cancelEdit();

        return true;
    }

    private void openUrl(String url) {
        openUrl(url, null, false);
    }

    private void openUrl(String url, boolean newTab) {
        openUrl(url, null, newTab);
    }

    private void openUrl(String url, String searchEngine) {
        openUrl(url, searchEngine, false);
    }

    private void openUrl(String url, String searchEngine, boolean newTab) {
        mBrowserToolbar.setProgressVisibility(true);

        int flags = Tabs.LOADURL_NONE;
        if (newTab) {
            flags |= Tabs.LOADURL_NEW_TAB;
        }

        Tabs.getInstance().loadUrl(url, searchEngine, -1, flags);

        hideBrowserSearch();
        mBrowserToolbar.cancelEdit();
    }

    private boolean isHomePagerVisible() {
        return (mHomePager != null && mHomePager.isVisible());
    }

    private void openReadingList() {
        Tabs.getInstance().loadUrl(ABOUT_HOME, Tabs.LOADURL_READING_LIST);
    }

    /* Favicon methods */
    private void loadFavicon(final Tab tab) {
        maybeCancelFaviconLoad(tab);

        int flags = LoadFaviconTask.FLAG_SCALE | ( (tab.isPrivate() || tab.getErrorType() != Tab.ErrorType.NONE) ? 0 : LoadFaviconTask.FLAG_PERSIST);
        int id = Favicons.loadFavicon(tab.getURL(), tab.getFaviconURL(), flags,
                        new OnFaviconLoadedListener() {

            @Override
            public void onFaviconLoaded(String pageUrl, Bitmap favicon) {
                // Leave favicon UI untouched if we failed to load the image
                // for some reason.
                if (favicon == null)
                    return;

                // The tab might be pointing to another URL by the time the
                // favicon is finally loaded, in which case we simply ignore it.
                if (!tab.getURL().equals(pageUrl))
                    return;

                tab.updateFavicon(favicon);
                tab.setFaviconLoadId(Favicons.NOT_LOADING);

                Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.FAVICON);
            }
        });

        tab.setFaviconLoadId(id);
    }

    private void maybeCancelFaviconLoad(Tab tab) {
        int faviconLoadId = tab.getFaviconLoadId();

        if (faviconLoadId == Favicons.NOT_LOADING)
            return;

        // Cancel pending favicon load task
        Favicons.cancelFaviconLoad(faviconLoadId);

        // Reset favicon load state
        tab.setFaviconLoadId(Favicons.NOT_LOADING);
    }

    /**
     * Enters editing mode with the current tab's URL. There might be no
     * tabs loaded by the time the user enters editing mode e.g. just after
     * the app starts. In this case, we simply fallback to an empty URL.
     */
    private void enterEditingMode() {
        String url = "";

        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            final String userSearch = tab.getUserSearch();

            // Check to see if there's a user-entered search term,
            // which we save whenever the user performs a search.
            url = (TextUtils.isEmpty(userSearch) ? tab.getURL() : userSearch);
        }

        enterEditingMode(url);
    }

    /**
     * Enters editing mode with the specified URL. This method will
     * always open the HISTORY page on about:home.
     */
    private void enterEditingMode(String url) {
        if (url == null) {
            throw new IllegalArgumentException("Cannot handle null URLs in enterEditingMode");
        }

        final PropertyAnimator animator = new PropertyAnimator(250);
        animator.setUseHardwareLayer(false);

        mBrowserToolbar.startEditing(url, animator);
        showHomePagerWithAnimator(HomePager.Page.HISTORY, animator);

        animator.start();
    }

    private void commitEditingMode() {
        if (!mBrowserToolbar.isEditing()) {
            return;
        }

        final String url = mBrowserToolbar.commitEdit();
        animateHideHomePager();
        hideBrowserSearch();

        // Don't do anything if the user entered an empty URL.
        if (TextUtils.isEmpty(url)) {
            return;
        }

        // If the URL doesn't look like a search query, just load it.
        if (!StringUtils.isSearchQuery(url, true)) {
            Tabs.getInstance().loadUrl(url, Tabs.LOADURL_USER_ENTERED);
            return;
        }

        // Otherwise, check for a bookmark keyword.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final String keyword;
                final String keywordSearch;

                final int index = url.indexOf(" ");
                if (index == -1) {
                    keyword = url;
                    keywordSearch = "";
                } else {
                    keyword = url.substring(0, index);
                    keywordSearch = url.substring(index + 1);
                }

                final String keywordUrl = BrowserDB.getUrlForKeyword(getContentResolver(), keyword);

                // If there isn't a bookmark keyword, just load the URL.
                if (TextUtils.isEmpty(keywordUrl)) {
                    Tabs.getInstance().loadUrl(url, Tabs.LOADURL_USER_ENTERED);
                    return;
                }

                recordSearch(null, "barkeyword");

                // Otherwise, construct a search query from the bookmark keyword.
                final String searchUrl = keywordUrl.replace("%s", URLEncoder.encode(keywordSearch));
                Tabs.getInstance().loadUrl(searchUrl, Tabs.LOADURL_USER_ENTERED);
            }
        });
    }

    /**
     * Record in Health Report that a search has occurred.
     *
     * @param identifier
     *        a search identifier, such as "partnername". Can be null.
     * @param where
     *        where the search was initialized; one of the values in
     *        {@link BrowserHealthRecorder#SEARCH_LOCATIONS}.
     */
    private static void recordSearch(String identifier, String where) {
        Log.i(LOGTAG, "Recording search: " + identifier + ", " + where);
        try {
            JSONObject message = new JSONObject();
            message.put("type", BrowserHealthRecorder.EVENT_SEARCH);
            message.put("location", where);
            message.put("identifier", identifier);
            GeckoAppShell.getEventDispatcher().dispatchEvent(message);
        } catch (Exception e) {
            Log.w(LOGTAG, "Error recording search.", e);
        }
    }

    private boolean dismissEditingMode() {
        if (!mBrowserToolbar.isEditing()) {
            return false;
        }

        mBrowserToolbar.cancelEdit();

        // Resetting the visibility of HomePager, which might have been hidden
        // by the filterEditingMode().
        mHomePager.setVisibility(View.VISIBLE);
        animateHideHomePager();
        hideBrowserSearch();

        return true;
    }

    void filterEditingMode(String searchTerm, AutocompleteHandler handler) {
        if (TextUtils.isEmpty(searchTerm)) {
            mHomePager.setVisibility(View.VISIBLE);
            hideBrowserSearch();
        } else {
            showBrowserSearch();
            mHomePager.setVisibility(View.INVISIBLE);
            mBrowserSearch.filter(searchTerm, handler);
        }
    }

    /**
     * Shows or hides the home pager for the given tab.
     */
    private void updateHomePagerForTab(Tab tab) {
        // Don't change the visibility of the home pager if we're in editing mode.
        if (mBrowserToolbar.isEditing()) {
            return;
        }

        if (isAboutHome(tab)) {
            showHomePager(tab.getAboutHomePage());

            if (isDynamicToolbarEnabled()) {
                // Show the toolbar.
                mLayerView.getLayerMarginsAnimator().showMargins(false);
            }
        } else {
            hideHomePager();
        }
    }

    private void showHomePager(HomePager.Page page) {
        showHomePagerWithAnimator(page, null);
    }

    private void showHomePagerWithAnimator(HomePager.Page page, PropertyAnimator animator) {
        if (isHomePagerVisible()) {
            return;
        }

        // Refresh toolbar height to possibly restore the toolbar padding
        refreshToolbarHeight();

        // Show the toolbar before hiding about:home so the
        // onMetricsChanged callback still works.
        if (isDynamicToolbarEnabled() && mLayerView != null) {
            mLayerView.getLayerMarginsAnimator().showMargins(true);
        }

        if (mHomePager == null) {
            final ViewStub homePagerStub = (ViewStub) findViewById(R.id.home_pager_stub);
            mHomePager = (HomePager) homePagerStub.inflate();
        }
        mHomePager.show(getSupportFragmentManager(), page, animator);
    }

    private void animateHideHomePager() {
        hideHomePagerWithAnimation(true);
    }

    private void hideHomePager() {
        hideHomePagerWithAnimation(false);
    }

    private void hideHomePagerWithAnimation(boolean animate) {
        if (!isHomePagerVisible()) {
            return;
        }

        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null && isAboutHome(tab)) {
            return;
        }

        // FIXME: do animation if animate is true
        if (mHomePager != null) {
            mHomePager.hide();
        }

        mBrowserToolbar.setNextFocusDownId(R.id.layer_view);

        // Refresh toolbar height to possibly restore the toolbar padding
        refreshToolbarHeight();
    }

    private void showBrowserSearch() {
        if (mBrowserSearch.getUserVisibleHint()) {
            return;
        }

        mBrowserSearchContainer.setVisibility(View.VISIBLE);

        getSupportFragmentManager().beginTransaction()
                .add(R.id.search_container, mBrowserSearch, BROWSER_SEARCH_TAG).commitAllowingStateLoss();
        mBrowserSearch.setUserVisibleHint(true);
    }

    private void hideBrowserSearch() {
        if (!mBrowserSearch.getUserVisibleHint()) {
            return;
        }

        mBrowserSearchContainer.setVisibility(View.INVISIBLE);

        getSupportFragmentManager().beginTransaction()
                .remove(mBrowserSearch).commitAllowingStateLoss();
        mBrowserSearch.setUserVisibleHint(false);
    }

    private class HideTabsTouchListener implements TouchEventInterceptor {
        private boolean mIsHidingTabs = false;

        @Override
        public boolean onInterceptTouchEvent(View view, MotionEvent event) {
            // We need to account for scroll state for the touched view otherwise
            // tapping on an "empty" part of the view will still be considered a
            // valid touch event.
            if (view.getScrollX() != 0 || view.getScrollY() != 0) {
                Rect rect = new Rect();
                view.getHitRect(rect);
                rect.offset(-view.getScrollX(), -view.getScrollY());

                int[] viewCoords = new int[2];
                view.getLocationOnScreen(viewCoords);

                int x = (int) event.getRawX() - viewCoords[0];
                int y = (int) event.getRawY() - viewCoords[1];

                if (!rect.contains(x, y))
                    return false;
            }

            // If the tab tray is showing, hide the tab tray and don't send the event to content.
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN && autoHideTabs()) {
                mIsHidingTabs = true;
                return true;
            }
            return false;
        }

        @Override
        public boolean onTouch(View view, MotionEvent event) {
            if (mIsHidingTabs) {
                // Keep consuming events until the gesture finishes.
                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                    mIsHidingTabs = false;
                }
                return true;
            }
            return false;
        }
    }

    private Menu findParentMenu(Menu menu, MenuItem item) {
        final int itemId = item.getItemId();

        final int count = (menu != null) ? menu.size() : 0;
        for (int i = 0; i < count; i++) {
            MenuItem menuItem = menu.getItem(i);
            if (menuItem.getItemId() == itemId) {
                return menu;
            }
            if (menuItem.hasSubMenu()) {
                Menu parent = findParentMenu(menuItem.getSubMenu(), item);
                if (parent != null) {
                    return parent;
                }
            }
        }

        return null;
    }

    private void addAddonMenuItem(final MenuItemInfo info) {
        if (mMenu == null) {
            if (mAddonMenuItemsCache == null)
                mAddonMenuItemsCache = new Vector<MenuItemInfo>();

            mAddonMenuItemsCache.add(info);
            return;
        }

        Menu menu;
        if (info.parent == 0) {
            menu = mMenu;
        } else if (info.parent == GECKO_TOOLS_MENU) {
            MenuItem tools = mMenu.findItem(R.id.tools);
            menu = tools != null ? tools.getSubMenu() : mMenu;
        } else {
            MenuItem parent = mMenu.findItem(info.parent);
            if (parent == null)
                return;

            Menu parentMenu = findParentMenu(mMenu, parent);

            if (!parent.hasSubMenu()) {
                parentMenu.removeItem(parent.getItemId());
                menu = parentMenu.addSubMenu(Menu.NONE, parent.getItemId(), Menu.NONE, parent.getTitle());
                if (parent.getIcon() != null)
                    ((SubMenu) menu).getItem().setIcon(parent.getIcon());
            } else {
                menu = parent.getSubMenu();
            }
        }

        MenuItem item = menu.add(Menu.NONE, info.id, Menu.NONE, info.label);
        item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                Log.i(LOGTAG, "menu item clicked");
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Menu:Clicked", Integer.toString(info.id - ADDON_MENU_OFFSET)));
                return true;
            }
        });

        if (info.icon != null) {
            final int id = info.id;
            BitmapUtils.getDrawable(this, info.icon, new BitmapUtils.BitmapLoader() {
                @Override
                public void onBitmapFound(Drawable d) {
                    MenuItem item = mMenu.findItem(id);
                    if (item == null) {
                        return;
                    }
                    if (d == null) {
                        item.setIcon(R.drawable.ic_menu_addons_filler);
                        return;
                    }
                    item.setIcon(d);
                }
            });
        } else {
            item.setIcon(R.drawable.ic_menu_addons_filler);
        }

        item.setCheckable(info.checkable);
        item.setChecked(info.checked);
        item.setEnabled(info.enabled);
        item.setVisible(info.visible);
    }

    private void removeAddonMenuItem(int id) {
        // Remove add-on menu item from cache, if available.
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 if (item.id == id) {
                     mAddonMenuItemsCache.remove(item);
                     break;
                 }
            }
        }

        if (mMenu == null)
            return;

        MenuItem menuItem = mMenu.findItem(id);
        if (menuItem != null)
            mMenu.removeItem(id);
    }

    private void updateAddonMenuItem(int id, JSONObject options) {
        // Set attribute for the menu item in cache, if available
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 if (item.id == id) {
                     item.checkable = options.optBoolean("checkable", item.checkable);
                     item.checked = options.optBoolean("checked", item.checked);
                     item.enabled = options.optBoolean("enabled", item.enabled);
                     item.visible = options.optBoolean("visible", item.visible);
                     break;
                 }
            }
        }

        if (mMenu == null)
            return;

        MenuItem menuItem = mMenu.findItem(id);
        if (menuItem != null) {
            menuItem.setCheckable(options.optBoolean("checkable", menuItem.isCheckable()));
            menuItem.setChecked(options.optBoolean("checked", menuItem.isChecked()));
            menuItem.setEnabled(options.optBoolean("enabled", menuItem.isEnabled()));
            menuItem.setVisible(options.optBoolean("visible", menuItem.isVisible()));
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);

        // Inform the menu about the action-items bar. 
        if (menu instanceof GeckoMenu && HardwareUtils.isTablet())
            ((GeckoMenu) menu).setActionItemBarPresenter(mBrowserToolbar);

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.browser_app_menu, mMenu);

        // Add add-on menu items if any.
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                 addAddonMenuItem(item);
            }

            mAddonMenuItemsCache.clear();
        }

        // Action providers are available only ICS+.
        if (Build.VERSION.SDK_INT >= 14) {
            MenuItem share = mMenu.findItem(R.id.share);
            GeckoActionProvider provider = new GeckoActionProvider(this);
            share.setActionProvider(provider);
        }

        return true;
    }

    @Override
    public void openOptionsMenu() {
        if (!hasTabsSideBar() && areTabsShown())
            return;

        // Scroll custom menu to the top
        if (mMenuPanel != null)
            mMenuPanel.scrollTo(0, 0);

        if (!mBrowserToolbar.openOptionsMenu())
            super.openOptionsMenu();

        if (isDynamicToolbarEnabled() && mLayerView != null)
            mLayerView.getLayerMarginsAnimator().showMargins(false);
    }

    @Override
    public void closeOptionsMenu() {
        if (!mBrowserToolbar.closeOptionsMenu())
            super.closeOptionsMenu();
    }

    @Override
    public void setFullScreen(final boolean fullscreen) {
        super.setFullScreen(fullscreen);
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (fullscreen) {
                    mBrowserToolbar.hide();
                    if (isDynamicToolbarEnabled()) {
                        mLayerView.getLayerMarginsAnimator().hideMargins(true);
                        mLayerView.getLayerMarginsAnimator().setMaxMargins(0, 0, 0, 0);
                    }
                } else {
                    mBrowserToolbar.show();
                    if (isDynamicToolbarEnabled()) {
                        mLayerView.getLayerMarginsAnimator().showMargins(true);
                        mLayerView.getLayerMarginsAnimator().setMaxMargins(0, mToolbarHeight, 0, 0);
                    }
                }
            }
        });
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu aMenu) {
        if (aMenu == null)
            return false;

        if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning))
            aMenu.findItem(R.id.settings).setEnabled(false);

        Tab tab = Tabs.getInstance().getSelectedTab();
        MenuItem bookmark = aMenu.findItem(R.id.bookmark);
        MenuItem forward = aMenu.findItem(R.id.forward);
        MenuItem share = aMenu.findItem(R.id.share);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);
        MenuItem charEncoding = aMenu.findItem(R.id.char_encoding);
        MenuItem findInPage = aMenu.findItem(R.id.find_in_page);
        MenuItem desktopMode = aMenu.findItem(R.id.desktop_mode);
        MenuItem enterGuestMode = aMenu.findItem(R.id.new_guest_session);
        MenuItem exitGuestMode = aMenu.findItem(R.id.exit_guest_session);

        // Only show the "Quit" menu item on pre-ICS or television devices.
        // In ICS+, it's easy to kill an app through the task switcher.
        aMenu.findItem(R.id.quit).setVisible(Build.VERSION.SDK_INT < 14 || HardwareUtils.isTelevision());

        if (tab == null || tab.getURL() == null) {
            bookmark.setEnabled(false);
            forward.setEnabled(false);
            share.setEnabled(false);
            saveAsPDF.setEnabled(false);
            findInPage.setEnabled(false);
            return true;
        }

        bookmark.setEnabled(!tab.getURL().startsWith("about:reader"));
        bookmark.setCheckable(true);
        bookmark.setChecked(tab.isBookmark());
        bookmark.setIcon(tab.isBookmark() ? R.drawable.ic_menu_bookmark_remove : R.drawable.ic_menu_bookmark_add);

        forward.setEnabled(tab.canDoForward());
        desktopMode.setChecked(tab.getDesktopMode());
        desktopMode.setIcon(tab.getDesktopMode() ? R.drawable.ic_menu_desktop_mode_on : R.drawable.ic_menu_desktop_mode_off);

        String url = tab.getURL();
        if (ReaderModeUtils.isAboutReader(url)) {
            String urlFromReader = ReaderModeUtils.getUrlFromAboutReader(url);
            if (urlFromReader != null)
                url = urlFromReader;
        }

        // Disable share menuitem for about:, chrome:, file:, and resource: URIs
        String scheme = Uri.parse(url).getScheme();
        share.setVisible(!GeckoProfile.get(this).inGuestMode());
        share.setEnabled(!(scheme.equals("about") || scheme.equals("chrome") ||
                           scheme.equals("file") || scheme.equals("resource")));

        // Action providers are available only ICS+.
        if (Build.VERSION.SDK_INT >= 14) {
            GeckoActionProvider provider = (GeckoActionProvider) share.getActionProvider();
            if (provider != null) {
                Intent shareIntent = provider.getIntent();

                // For efficiency, the provider's intent is only set once
                if (shareIntent == null) {
                    shareIntent = new Intent(Intent.ACTION_SEND);
                    shareIntent.setType("text/plain");
                    provider.setIntent(shareIntent);
                }

                // Replace the existing intent's extras
                shareIntent.putExtra(Intent.EXTRA_TEXT, url);
                shareIntent.putExtra(Intent.EXTRA_SUBJECT, tab.getDisplayTitle());
                shareIntent.putExtra(Intent.EXTRA_TITLE, tab.getDisplayTitle());

                // Clear the existing thumbnail extras so we don't share an old thumbnail.
                shareIntent.removeExtra("share_screenshot_uri");

                // Include the thumbnail of the page being shared.
                BitmapDrawable drawable = tab.getThumbnail();
                if (drawable != null) {
                    Bitmap thumbnail = drawable.getBitmap();

                    // Kobo uses a custom intent extra for sharing thumbnails.
                    if (Build.MANUFACTURER.equals("Kobo") && thumbnail != null) {
                        File cacheDir = getExternalCacheDir();

                        if (cacheDir != null) {
                            File outFile = new File(cacheDir, "thumbnail.png");

                            try {
                                java.io.FileOutputStream out = new java.io.FileOutputStream(outFile);
                                thumbnail.compress(Bitmap.CompressFormat.PNG, 90, out);
                            } catch (FileNotFoundException e) {
                                Log.e(LOGTAG, "File not found", e);
                            }

                            shareIntent.putExtra("share_screenshot_uri", Uri.parse(outFile.getPath()));
                        }
                    }
                }
            }
        }

        // Disable save as PDF for about:home and xul pages
        saveAsPDF.setEnabled(!(tab.getURL().equals("about:home") ||
                               tab.getContentType().equals("application/vnd.mozilla.xul+xml")));

        // Disable find in page for about:home, since it won't work on Java content
        findInPage.setEnabled(!isAboutHome(tab));

        charEncoding.setVisible(GeckoPreferences.getCharEncodingState());

        if (mProfile.inGuestMode())
            exitGuestMode.setVisible(true);
        else
            enterGuestMode.setVisible(true);

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Tab tab = null;
        Intent intent = null;

        final int itemId = item.getItemId();

        if (itemId == R.id.bookmark) {
            tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                if (item.isChecked()) {
                    tab.removeBookmark();
                    Toast.makeText(this, R.string.bookmark_removed, Toast.LENGTH_SHORT).show();
                    item.setIcon(R.drawable.ic_menu_bookmark_add);
                } else {
                    tab.addBookmark();
                    mToast.show(false,
                        getResources().getString(R.string.bookmark_added),
                        getResources().getString(R.string.bookmark_options),
                        null,
                        new ButtonToast.ToastListener() {
                            @Override
                            public void onButtonClicked() {
                                showBookmarkDialog();
                            }

                            @Override
                            public void onToastHidden(ButtonToast.ReasonHidden reason) { }
                        });
                    item.setIcon(R.drawable.ic_menu_bookmark_remove);
                }
            }
            return true;
        }

        if (itemId == R.id.share) {
            shareCurrentUrl();
            return true;
        }

        if (itemId == R.id.reload) {
            tab = Tabs.getInstance().getSelectedTab();
            if (tab != null)
                tab.doReload();
            return true;
        }

        if (itemId == R.id.forward) {
            tab = Tabs.getInstance().getSelectedTab();
            if (tab != null)
                tab.doForward();
            return true;
        }

        if (itemId == R.id.save_as_pdf) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SaveAs:PDF", null));
            return true;
        }

        if (itemId == R.id.settings) {
            intent = new Intent(this, GeckoPreferences.class);
            startActivity(intent);
            return true;
        }

        if (itemId == R.id.addons) {
            Tabs.getInstance().loadUrlInTab("about:addons");
            return true;
        }

        if (itemId == R.id.downloads) {
            Tabs.getInstance().loadUrlInTab("about:downloads");
            return true;
        }

        if (itemId == R.id.apps) {
            Tabs.getInstance().loadUrlInTab("about:apps");
            return true;
        }

        if (itemId == R.id.char_encoding) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("CharEncoding:Get", null));
            return true;
        }

        if (itemId == R.id.find_in_page) {
            mFindInPageBar.show();
            return true;
        }

        if (itemId == R.id.desktop_mode) {
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab == null)
                return true;
            JSONObject args = new JSONObject();
            try {
                args.put("desktopMode", !item.isChecked());
                args.put("tabId", selectedTab.getId());
            } catch (JSONException e) {
                Log.e(LOGTAG, "error building json arguments");
            }
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("DesktopMode:Change", args.toString()));
            return true;
        }

        if (itemId == R.id.new_tab) {
            addTab();
            return true;
        }

        if (itemId == R.id.new_private_tab) {
            addPrivateTab();
            return true;
        }

        if (itemId == R.id.new_guest_session) {
            showGuestModeDialog(GuestModeDialog.ENTERING);
            return true;
        }

        if (itemId == R.id.exit_guest_session) {
            showGuestModeDialog(GuestModeDialog.LEAVING);
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void showGuestModeDialog(final GuestModeDialog type) {
        final Prompt ps = new Prompt(this, new Prompt.PromptCallback() {
            @Override
            public void onPromptFinished(String result) {
                try {
                    int itemId = new JSONObject(result).getInt("button");
                    if (itemId == 0) {
                        String args = "";
                        if (type == GuestModeDialog.ENTERING) {
                            args = GUEST_BROWSING_ARG;
                        } else {
                            GeckoProfile.leaveGuestSession(BrowserApp.this);
                        }
                        doRestart(args);
                        System.exit(0);
                    }
                } catch(JSONException ex) {
                    Log.e(LOGTAG, "Exception reading guest mode prompt result", ex);
                }
            }
        });

        Resources res = getResources();
        ps.setButtons(new String[] {
            res.getString(R.string.guest_session_dialog_continue),
            res.getString(R.string.guest_session_dialog_cancel)
        });

        int titleString = 0;
        int msgString = 0;
        if (type == GuestModeDialog.ENTERING) {
            titleString = R.string.new_guest_session_title;
            msgString = R.string.new_guest_session_text;
        } else {
            titleString = R.string.exit_guest_session_title;
            msgString = R.string.exit_guest_session_text;
        }

        ps.show(res.getString(titleString), res.getString(msgString), null, false);
    }

    /**
     * This will detect if the key pressed is back. If so, will show the history.
     */
    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                return tab.showAllHistory();
            }
        }
        return super.onKeyLongPress(keyCode, event);
    }

    /*
     * If the app has been launched a certain number of times, and we haven't asked for feedback before,
     * open a new tab with about:feedback when launching the app from the icon shortcut.
     */ 
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        String action = intent.getAction();

        if (AppConstants.MOZ_ANDROID_BEAM && Build.VERSION.SDK_INT >= 10 && NfcAdapter.ACTION_NDEF_DISCOVERED.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        }

        if (!mInitialized) {
            return;
        }

        // Dismiss editing mode if the user is loading a URL from an external app.
        if (Intent.ACTION_VIEW.equals(action)) {
            dismissEditingMode();
            return;
        }

        // Only solicit feedback when the app has been launched from the icon shortcut.
        if (!Intent.ACTION_MAIN.equals(action)) {
            return;
        }

        (new UiAsyncTask<Void, Void, Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized Boolean doInBackground(Void... params) {
                // Check to see how many times the app has been launched.
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                String keyName = getPackageName() + ".feedback_launch_count";
                int launchCount = settings.getInt(keyName, 0);
                if (launchCount >= FEEDBACK_LAUNCH_COUNT)
                    return false;

                // Increment the launch count and store the new value.
                launchCount++;
                settings.edit().putInt(keyName, launchCount).commit();

                // If we've reached our magic number, show the feedback page.
                return launchCount == FEEDBACK_LAUNCH_COUNT;
            }

            @Override
            public void onPostExecute(Boolean shouldShowFeedbackPage) {
                if (shouldShowFeedbackPage)
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Feedback:Show", null));
            }
        }).execute();
    }

    @Override
    protected NotificationClient makeNotificationClient() {
        // The service is local to Fennec, so we can use it to keep
        // Fennec alive during downloads.
        return new ServiceNotificationClient(getApplicationContext());
    }

    private void resetFeedbackLaunchCount() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public synchronized void run() {
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                settings.edit().putInt(getPackageName() + ".feedback_launch_count", 0).commit();
            }
        });
    }

    private void getLastUrl() {
        (new UiAsyncTask<Void, Void, String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized String doInBackground(Void... params) {
                // Get the most recent URL stored in browser history.
                String url = "";
                Cursor c = null;
                try {
                    c = BrowserDB.getRecentHistory(getContentResolver(), 1);
                    if (c.moveToFirst()) {
                        url = c.getString(c.getColumnIndexOrThrow(Combined.URL));
                    }
                } finally {
                    if (c != null)
                        c.close();
                }
                return url;
            }

            @Override
            public void onPostExecute(String url) {
                // Don't bother sending a message if there is no URL.
                if (url.length() > 0)
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Feedback:LastUrl", url));
            }
        }).execute();
    }

    // HomePager.OnNewTabsListener
    @Override
    public void onNewTabs(String[] urls) {
        for (String url : urls) {
            openUrl(url, true);
        }
    }

    // HomePager.OnUrlOpenListener
    @Override
    public void onUrlOpen(String url, EnumSet<OnUrlOpenListener.Flags> flags) {
        if (!maybeSwitchToTab(url, flags)) {
            openUrl(url);
        }
    }

    // BrowserSearch.OnSearchListener
    @Override
    public void onSearch(String engineId, String text) {
        recordSearch(engineId, "barsuggest");
        openUrl(text, engineId);
    }

    // BrowserSearch.OnEditSuggestionListener
    @Override
    public void onEditSuggestion(String suggestion) {
        mBrowserToolbar.onEditSuggestion(suggestion);
    }

    @Override
    public int getLayout() { return R.layout.gecko_app; }

    @Override
    protected String getDefaultProfileName() {
        String profile = GeckoProfile.findDefaultProfile(this);
        return (profile != null ? profile : "default");
    }

    /**
     * Launch UI that lets the user update Firefox.
     *
     * This depends on the current channel: Release and Beta both direct to the
     * Google Play Store.  If updating is enabled, Aurora, Nightly, and custom
     * builds open about:, which provides an update interface.
     *
     * If updating is not enabled, this simply logs an error.
     *
     * @return true if update UI was launched.
     */
    protected boolean handleUpdaterLaunch() {
        if (AppConstants.RELEASE_BUILD) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse("market://details?id=" + getPackageName()));
            startActivity(intent);
            return true;
        }

        if (AppConstants.MOZ_UPDATER) {
            Tabs.getInstance().loadUrlInTab("about:");
            return true;
        }

        Log.w(LOGTAG, "No candidate updater found; ignoring launch request.");
        return false;
    }
}
