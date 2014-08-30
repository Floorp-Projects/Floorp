/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.io.FileNotFoundException;
import java.lang.reflect.Method;
import java.net.URLEncoder;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Vector;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.DynamicToolbar.PinReason;
import org.mozilla.gecko.DynamicToolbar.VisibilityTransition;
import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.LoadFaviconTask;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.favicons.decoders.IconDirectoryEntry;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.activities.FxAccountGetStartedActivity;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerMarginsAnimator;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.health.BrowserHealthRecorder;
import org.mozilla.gecko.health.BrowserHealthReporter;
import org.mozilla.gecko.health.HealthRecorder;
import org.mozilla.gecko.health.SessionInformation;
import org.mozilla.gecko.home.BrowserSearch;
import org.mozilla.gecko.home.HomeBanner;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.HomePanelsManager;
import org.mozilla.gecko.home.SearchEngine;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuItem;
import org.mozilla.gecko.preferences.ClearOnShutdownPref;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.prompts.Prompt;
import org.mozilla.gecko.prompts.PromptListItem;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.tabs.TabsPanel;
import org.mozilla.gecko.toolbar.AutocompleteHandler;
import org.mozilla.gecko.toolbar.BrowserToolbar;
import org.mozilla.gecko.toolbar.ToolbarProgressView;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.MenuUtils;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.PrefUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;
import org.mozilla.gecko.widget.ButtonToast;
import org.mozilla.gecko.widget.GeckoActionProvider;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
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
import android.support.v4.app.FragmentManager;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.animation.Interpolator;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.Toast;
import android.widget.ViewFlipper;

public class BrowserApp extends GeckoApp
                        implements TabsPanel.TabsLayoutChangeListener,
                                   PropertyAnimator.PropertyAnimationListener,
                                   View.OnKeyListener,
                                   LayerView.OnMetricsChangedListener,
                                   BrowserSearch.OnSearchListener,
                                   BrowserSearch.OnEditSuggestionListener,
                                   HomePager.OnNewTabsListener,
                                   OnUrlOpenListener,
                                   ActionModeCompat.Presenter {
    private static final String LOGTAG = "GeckoBrowserApp";

    private static final int TABS_ANIMATION_DURATION = 450;

    private static final int READER_ADD_SUCCESS = 0;
    private static final int READER_ADD_FAILED = 1;
    private static final int READER_ADD_DUPLICATE = 2;

    private static final String ADD_SHORTCUT_TOAST = "add_shortcut_toast";
    public static final String GUEST_BROWSING_ARG = "--guest";

    private static final String STATE_ABOUT_HOME_TOP_PADDING = "abouthome_top_padding";

    private static final String BROWSER_SEARCH_TAG = "browser_search";

    // Request ID for startActivityForResult.
    private static final int ACTIVITY_REQUEST_PREFERENCES = 1001;
    public static final String ACTION_NEW_PROFILE = "org.mozilla.gecko.NEW_PROFILE";

    private BrowserSearch mBrowserSearch;
    private View mBrowserSearchContainer;

    public ViewFlipper mViewFlipper;
    public ActionModeCompatView mActionBar;
    private BrowserToolbar mBrowserToolbar;
    private ToolbarProgressView mProgressView;
    private HomePager mHomePager;
    private TabsPanel mTabsPanel;
    private ViewGroup mHomePagerContainer;
    protected Telemetry.Timer mAboutHomeStartupTimer;
    private ActionModeCompat mActionMode;
    private boolean mShowActionModeEndAnimation;

    private static final int GECKO_TOOLS_MENU = -1;
    private static final int ADDON_MENU_OFFSET = 1000;
    private static class MenuItemInfo {
        public int id;
        public String label;
        public String icon;
        public boolean checkable;
        public boolean checked;
        public boolean enabled = true;
        public boolean visible = true;
        public int parent;
        public boolean added;   // So we can re-add after a locale change.
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
    private MediaCastingBar mMediaCastingBar;

    // We'll ask for feedback after the user launches the app this many times.
    private static final int FEEDBACK_LAUNCH_COUNT = 15;

    // Stored value of the toolbar height, so we know when it's changed.
    private int mToolbarHeight;

    // Stored value of whether the last metrics change allowed for toolbar
    // scrolling.
    private boolean mDynamicToolbarCanScroll;

    private SharedPreferencesHelper mSharedPreferencesHelper;

    private OrderedBroadcastHelper mOrderedBroadcastHelper;

    private BroadcastReceiver mOnboardingReceiver;

    private BrowserHealthReporter mBrowserHealthReporter;

    // The tab to be selected on editing mode exit.
    private Integer mTargetTabForEditingMode;

    // The animator used to toggle HomePager visibility has a race where if the HomePager is shown
    // (starting the animation), the HomePager is hidden, and the HomePager animation completes,
    // both the web content and the HomePager will be hidden. This flag is used to prevent the
    // race by determining if the web content should be hidden at the animation's end.
    private boolean mHideWebContentOnAnimationEnd;

    private DynamicToolbar mDynamicToolbar = new DynamicToolbar();

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        if (tab == null) {
            // Only RESTORED is allowed a null tab: it's the only event that
            // isn't tied to a specific tab.
            if (msg != Tabs.TabEvents.RESTORED) {
                throw new IllegalArgumentException("onTabChanged:" + msg + " must specify a tab.");
            }
            return;
        }

        Log.d(LOGTAG, "BrowserApp.onTabChanged: " + tab.getId() + ": " + msg);
        switch(msg) {
            case LOCATION_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    maybeCancelFaviconLoad(tab);
                }
                // fall through
            case SELECTED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateHomePagerForTab(tab);
                }
                break;
            case START:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    invalidateOptionsMenu();

                    if (mDynamicToolbar.isEnabled()) {
                        mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
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
        if (AndroidGamepadManager.handleKeyEvent(event)) {
            return true;
        }

        // Global onKey handler. This is called if the focused UI doesn't
        // handle the key event, and before Gecko swallows the events.
        if (event.getAction() != KeyEvent.ACTION_DOWN) {
            return false;
        }

        if ((event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_BUTTON_Y:
                    // Toggle/focus the address bar on gamepad-y button.
                    if (mViewFlipper.getVisibility() == View.VISIBLE) {
                        if (mDynamicToolbar.isEnabled() && !isHomePagerVisible()) {
                            mDynamicToolbar.setVisible(false, VisibilityTransition.ANIMATE);
                            if (mLayerView != null) {
                                mLayerView.requestFocus();
                            }
                        } else {
                            // Just focus the address bar when about:home is visible
                            // or when the dynamic toolbar isn't enabled.
                            mBrowserToolbar.requestFocusFromTouch();
                        }
                    } else {
                        mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
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
        if (Versions.feature11Plus && tab != null && event.isCtrlPressed()) {
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
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (AndroidGamepadManager.handleKeyEvent(event)) {
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    void handleReaderListStatusRequest(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final int inReadingList = BrowserDB.isReadingListItem(getContentResolver(), url) ? 1 : 0;

                final JSONObject json = new JSONObject();
                try {
                    json.put("url", url);
                    json.put("inReadingList", inReadingList);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON error - failed to return inReadingList status", e);
                    return;
                }

                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:ListStatusReturn", json.toString()));
            }
        });
    }

    private void handleReaderAdded(int result, final ContentValues values) {
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
                BrowserDB.addReadingListItem(getContentResolver(), values);
                showToast(R.string.reading_list_added, Toast.LENGTH_SHORT);
            }
        });
    }

    private ContentValues messageToReadingListContentValues(JSONObject message) {
        final ContentValues values = new ContentValues();
        values.put(ReadingListItems.URL, message.optString("url"));
        values.put(ReadingListItems.TITLE, message.optString("title"));
        values.put(ReadingListItems.LENGTH, message.optInt("length"));
        values.put(ReadingListItems.EXCERPT, message.optString("excerpt"));
        return values;
    }

    void handleReaderRemoved(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.removeReadingListItemWithURL(getContentResolver(), url);
                showToast(R.string.page_removed, Toast.LENGTH_SHORT);
            }
        });
    }

    private void handleReaderFaviconRequest(final String url) {
        (new UIAsyncTask.WithoutParams<String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public String doInBackground() {
                return Favicons.getFaviconURLForPageURL(url);
            }

            @Override
            public void onPostExecute(String faviconUrl) {
                JSONObject args = new JSONObject();

                if (faviconUrl != null) {
                    try {
                        args.put("url", url);
                        args.put("faviconUrl", faviconUrl);
                    } catch (JSONException e) {
                        Log.w(LOGTAG, "Error building JSON favicon arguments.", e);
                    }
                }

                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:FaviconReturn", args.toString()));
            }
        }).execute();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mAboutHomeStartupTimer = new Telemetry.UptimeTimer("FENNEC_STARTUP_TIME_ABOUTHOME");

        final Intent intent = getIntent();

        String args = intent.getStringExtra("args");
        if (args != null && args.contains(GUEST_BROWSING_ARG)) {
            mProfile = GeckoProfile.createGuestProfile(this);
        } else {
            GeckoProfile.maybeCleanupGuestProfile(this);
        }

        // This has to be prepared prior to calling GeckoApp.onCreate, because
        // widget code and BrowserToolbar need it, and they're created by the
        // layout, which GeckoApp takes care of.
        ((GeckoApplication) getApplication()).prepareLightweightTheme();
        super.onCreate(savedInstanceState);

        final Context appContext = getApplicationContext();

        mViewFlipper = (ViewFlipper) findViewById(R.id.browser_actionbar);
        mActionBar = (ActionModeCompatView) findViewById(R.id.actionbar);

        mBrowserToolbar = (BrowserToolbar) findViewById(R.id.browser_toolbar);
        mProgressView = (ToolbarProgressView) findViewById(R.id.progress);
        mBrowserToolbar.setProgressBar(mProgressView);
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            // Show the target URL immediately in the toolbar.
            mBrowserToolbar.setTitle(intent.getDataString());

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT);
        }

        ((GeckoApp.MainLayout) mMainLayout).setTouchEventInterceptor(new HideOnTouchListener());
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

        mHomePagerContainer = (ViewGroup) findViewById(R.id.home_pager_container);

        mBrowserSearchContainer = findViewById(R.id.search_container);
        mBrowserSearch = (BrowserSearch) getSupportFragmentManager().findFragmentByTag(BROWSER_SEARCH_TAG);
        if (mBrowserSearch == null) {
            mBrowserSearch = BrowserSearch.newInstance();
            mBrowserSearch.setUserVisibleHint(false);
        }

        setBrowserToolbarListeners();

        mFindInPageBar = (FindInPageBar) findViewById(R.id.find_in_page);
        mMediaCastingBar = (MediaCastingBar) findViewById(R.id.media_casting);

        EventDispatcher.getInstance().registerGeckoThreadListener((GeckoEventListener)this,
            "Menu:Update",
            "Reader:Added",
            "Reader:FaviconRequest",
            "Search:Keyword",
            "Prompt:ShowTop",
            "Accounts:Exist");

        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener)this,
            "Accounts:Create",
            "CharEncoding:Data",
            "CharEncoding:State",
            "Feedback:LastUrl",
            "Feedback:MaybeLater",
            "Feedback:OpenPlayStore",
            "Menu:Add",
            "Menu:Remove",
            "Reader:ListStatusRequest",
            "Reader:Removed",
            "Reader:Share",
            "Settings:Show",
            "Telemetry:Gather",
            "Updater:Launch",
            "BrowserToolbar:Visibility");

        registerOnboardingReceiver(this);

        Distribution distribution = Distribution.init(this);

        // Init suggested sites engine in BrowserDB.
        final SuggestedSites suggestedSites = new SuggestedSites(appContext, distribution);
        BrowserDB.setSuggestedSites(suggestedSites);

        JavaAddonManager.getInstance().init(appContext);
        mSharedPreferencesHelper = new SharedPreferencesHelper(appContext);
        mOrderedBroadcastHelper = new OrderedBroadcastHelper(appContext);
        mBrowserHealthReporter = new BrowserHealthReporter();

        if (AppConstants.MOZ_ANDROID_BEAM) {
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
            mDynamicToolbar.onRestoreInstanceState(savedInstanceState);
            mHomePagerContainer.setPadding(0, savedInstanceState.getInt(STATE_ABOUT_HOME_TOP_PADDING), 0, 0);
        }

        mDynamicToolbar.setEnabledChangedListener(new DynamicToolbar.OnEnabledChangedListener() {
            @Override
            public void onEnabledChanged(boolean enabled) {
                setDynamicToolbarEnabled(enabled);
            }
        });

        // Set the maximum bits-per-pixel the favicon system cares about.
        IconDirectoryEntry.setMaxBPP(GeckoAppShell.getScreenDepth());

        Class<?> mediaManagerClass = getMediaPlayerManager();
        if (mediaManagerClass != null) {
            try {
                Method init = mediaManagerClass.getMethod("init", Context.class);
                init.invoke(null, this);
            } catch(Exception ex) {
                Log.i(LOGTAG, "Error initializing media manager", ex);
            }
        }
    }

    private void registerOnboardingReceiver(Context context) {
        final LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(this);

        // Receiver for launching first run start pane on new profile creation.
        mOnboardingReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                launchStartPane(BrowserApp.this);
            }
        };

        lbm.registerReceiver(mOnboardingReceiver, new IntentFilter(ACTION_NEW_PROFILE));
    }

    private void launchStartPane(Context context) {
         final Intent startIntent = new Intent(context, StartPane.class);
         context.startActivity(startIntent);
     }

    private Class<?> getMediaPlayerManager() {
        if (AppConstants.MOZ_MEDIA_PLAYER) {
            try {
                return Class.forName("org.mozilla.gecko.MediaPlayerManager");
            } catch(Exception ex) {
                // Ignore failures
                Log.i(LOGTAG, "No native casting support", ex);
            }
        }

        return null;
    }


    @Override
    public void onBackPressed() {
        if (getSupportFragmentManager().getBackStackEntryCount() > 0) {
            super.onBackPressed();
            return;
        }

        if (mBrowserToolbar.onBackPressed()) {
            return;
        }

        if (mActionMode != null) {
            endActionModeCompat();
            return;
        }

        super.onBackPressed();
    }

    @Override
    public void onResume() {
        super.onResume();
        EventDispatcher.getInstance().unregisterGeckoThreadListener((GeckoEventListener)this,
            "Prompt:ShowTop");
        if (AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED) {
            // Starts or pings the stumbler lib, see also usage in handleMessage(): Gecko:DelayedStartup.
            GeckoPreferences.broadcastStumblerPref(this);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        // Register for Prompt:ShowTop so we can foreground this activity even if it's hidden.
        EventDispatcher.getInstance().registerGeckoThreadListener((GeckoEventListener)this,
            "Prompt:ShowTop");

        final LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(this);
        lbm.unregisterReceiver(mOnboardingReceiver);
    }

    private void setBrowserToolbarListeners() {
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
                mBrowserToolbar.cancelEdit();
            }
        });

        mBrowserToolbar.setOnFilterListener(new BrowserToolbar.OnFilterListener() {
            public void onFilter(String searchText, AutocompleteHandler handler) {
                filterEditingMode(searchText, handler);
            }
        });

        mBrowserToolbar.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (isHomePagerVisible()) {
                    mHomePager.onToolbarFocusChange(hasFocus);
                }
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
                selectTargetTabForEditingMode();

                // Since the underlying LayerView is set visible in hideHomePager, we would
                // ordinarily want to call it first. However, hideBrowserSearch changes the
                // visibility of the HomePager and hideHomePager will take no action if the
                // HomePager is hidden, so we want to call hideBrowserSearch to restore the
                // HomePager visibility first.
                hideBrowserSearch();
                hideHomePager();

                // Re-enable doorhanger notifications. They may trigger on the selected tab above.
                mDoorHangerPopup.enable();
            }
        });

        // Intercept key events for gamepad shortcuts
        mBrowserToolbar.setOnKeyListener(this);
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
                        GeckoAppShell.createShortcut(title, url, favicon);
                    }
                }
            }
        });

        final PromptListItem[] items = new PromptListItem[2];
        Resources res = getResources();
        items[0] = new PromptListItem(res.getString(R.string.contextmenu_edit_bookmark));
        items[1] = new PromptListItem(res.getString(R.string.contextmenu_add_to_launcher));

        ps.show("", "", items, ListView.CHOICE_MODE_NONE);
    }

    private void setDynamicToolbarEnabled(boolean enabled) {
        ThreadUtils.assertOnUiThread();

        if (enabled) {
            if (mLayerView != null) {
                mLayerView.setOnMetricsChangedListener(this);
            }
            setToolbarMargin(0);
            mHomePagerContainer.setPadding(0, mViewFlipper.getHeight(), 0, 0);
        } else {
            // Immediately show the toolbar when disabling the dynamic
            // toolbar.
            if (mLayerView != null) {
                mLayerView.setOnMetricsChangedListener(null);
            }
            mHomePagerContainer.setPadding(0, 0, 0, 0);
            if (mViewFlipper != null) {
                ViewHelper.setTranslationY(mViewFlipper, 0);
            }
        }

        refreshToolbarHeight();
    }

    private static boolean isAboutHome(final Tab tab) {
        return AboutPages.isAboutHome(tab.getURL());
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
                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.CONTEXT_MENU);
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "pasteandgo");
            }
            return true;
        }

        if (itemId == R.id.site_settings) {
            // This can be selected from either the browser menu or the contextmenu, depending on the size and version (v11+) of the phone.
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Permissions:Get", null));
            if (Versions.preHC) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "site_settings");
            }
            return true;
        }

        if (itemId == R.id.paste) {
            String text = Clipboard.getText();
            if (!TextUtils.isEmpty(text)) {
                enterEditingMode(text);
                showBrowserSearch();
                mBrowserSearch.filter(text, null);
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "paste");
            }
            return true;
        }

        if (itemId == R.id.subscribe) {
            // This can be selected from either the browser menu or the contextmenu, depending on the size and version (v11+) of the phone.
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && tab.hasFeeds()) {
                JSONObject args = new JSONObject();
                try {
                    args.put("tabId", tab.getId());
                } catch (JSONException e) {
                    Log.e(LOGTAG, "error building json arguments");
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Feeds:Subscribe", args.toString()));
                if (Versions.preHC) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "subscribe");
                }
            }
            return true;
        }

        if (itemId == R.id.add_search_engine) {
            // This can be selected from either the browser menu or the contextmenu, depending on the size and version (v11+) of the phone.
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && tab.hasOpenSearch()) {
                JSONObject args = new JSONObject();
                try {
                    args.put("tabId", tab.getId());
                } catch (JSONException e) {
                    Log.e(LOGTAG, "error building json arguments");
                    return true;
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:Add", args.toString()));

                if (Versions.preHC) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "add_search_engine");
                }
            }
            return true;
        }

        if (itemId == R.id.copyurl) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                String url = tab.getURL();
                if (url != null) {
                    Clipboard.setText(url);
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "copyurl");
                }
            }
            return true;
        }

        if (itemId == R.id.add_to_launcher) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab == null) {
                return true;
            }

            final String url = tab.getURL();
            final String title = tab.getDisplayTitle();
            if (url == null || title == null) {
                return true;
            }

            final OnFaviconLoadedListener listener = new GeckoAppShell.CreateShortcutFaviconLoadedListener(url, title);
            Favicons.getSizedFavicon(url,
                    tab.getFaviconURL(),
                    Integer.MAX_VALUE,
                    LoadFaviconTask.FLAG_PERSIST,
                    listener);
            return true;
        }

        return false;
    }

    @Override
    public void setAccessibilityEnabled(boolean enabled) {
        mDynamicToolbar.setAccessibilityEnabled(enabled);
    }

    @Override
    public void onDestroy() {
        mDynamicToolbar.destroy();

        if (mBrowserToolbar != null)
            mBrowserToolbar.onDestroy();

        if (mFindInPageBar != null) {
            mFindInPageBar.onDestroy();
            mFindInPageBar = null;
        }

        if (mMediaCastingBar != null) {
            mMediaCastingBar.onDestroy();
            mMediaCastingBar = null;
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

        EventDispatcher.getInstance().unregisterGeckoThreadListener((GeckoEventListener)this,
            "Menu:Update",
            "Reader:Added",
            "Reader:FaviconRequest",
            "Search:Keyword",
            "Prompt:ShowTop",
            "Accounts:Exist");

        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener)this,
            "Accounts:Create",
            "CharEncoding:Data",
            "CharEncoding:State",
            "Feedback:LastUrl",
            "Feedback:MaybeLater",
            "Feedback:OpenPlayStore",
            "Menu:Add",
            "Menu:Remove",
            "Reader:ListStatusRequest",
            "Reader:Removed",
            "Reader:Share",
            "Settings:Show",
            "Telemetry:Gather",
            "Updater:Launch",
            "BrowserToolbar:Visibility");

        if (AppConstants.MOZ_ANDROID_BEAM) {
            NfcAdapter nfc = NfcAdapter.getDefaultAdapter(this);
            if (nfc != null) {
                // null this out even though the docs say it's not needed,
                // because the source code looks like it will only do this
                // automatically on API 14+
                nfc.setNdefPushMessageCallback(null, this);
            }
        }

        Class<?> mediaManagerClass = getMediaPlayerManager();
        if (mediaManagerClass != null) {
            try {
                Method destroy = mediaManagerClass.getMethod("onDestroy",  (Class[]) null);
                destroy.invoke(null);
            } catch(Exception ex) {
                Log.i(LOGTAG, "Error destroying media manager", ex);
            }
        }

        super.onDestroy();
    }

    @Override
    protected void initializeChrome() {
        super.initializeChrome();

        mDoorHangerPopup.setAnchor(mBrowserToolbar.getDoorHangerAnchor());

        mDynamicToolbar.setLayerView(mLayerView);
        setDynamicToolbarEnabled(mDynamicToolbar.isEnabled());

        // Intercept key events for gamepad shortcuts
        mLayerView.setOnKeyListener(this);

        // Initialize the actionbar menu items on startup for both large and small tablets
        if (HardwareUtils.isTablet()) {
            onCreatePanelMenu(Window.FEATURE_OPTIONS_PANEL, null);
            invalidateOptionsMenu();
        }
    }

    private void shareCurrentUrl() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null) {
            return;
        }

        String url = tab.getURL();
        if (url == null) {
            return;
        }

        if (AboutPages.isAboutReader(url)) {
            url = ReaderModeUtils.getUrlFromAboutReader(url);
        }

        GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                      Intent.ACTION_SEND, tab.getDisplayTitle());

        // Context: Sharing via chrome list (no explicit session is active)
        Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST);
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
        if (isHomePagerVisible() || mViewFlipper == null) {
            return;
        }

        // If the page has shrunk so that the toolbar no longer scrolls, make
        // sure the toolbar is visible.
        if (aMetrics.getPageHeight() <= aMetrics.getHeight()) {
            if (mDynamicToolbarCanScroll) {
                mDynamicToolbarCanScroll = false;
                if (mViewFlipper.getVisibility() != View.VISIBLE) {
                    ThreadUtils.postToUiThread(new Runnable() {
                        public void run() {
                            mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
                        }
                    });
                }
            }
        } else {
            mDynamicToolbarCanScroll = true;
        }

        final View toolbarLayout = mViewFlipper;
        final ToolbarProgressView progressView = mProgressView;
        final int marginTop = Math.round(aMetrics.marginTop);
        ThreadUtils.postToUiThread(new Runnable() {
            public void run() {
                final float translationY = marginTop - toolbarLayout.getHeight();
                ViewHelper.setTranslationY(toolbarLayout, translationY);
                ViewHelper.setTranslationY(progressView, translationY);

                if (mDoorHangerPopup.isShowing()) {
                    mDoorHangerPopup.updatePopup();
                }
            }
        });

        if (mFormAssistPopup != null)
            mFormAssistPopup.onMetricsChanged(aMetrics);
    }

    @Override
    public void onPanZoomStopped() {
        if (!mDynamicToolbar.isEnabled() || isHomePagerVisible()) {
            return;
        }

        // Make sure the toolbar is fully hidden or fully shown when the user
        // lifts their finger. If the page is shorter than the viewport or if
        // the user has reached the end of the page, the toolbar is always
        // shown.
        ImmutableViewportMetrics metrics = mLayerView.getViewportMetrics();
        if (metrics.getPageHeight() < metrics.getHeight()
              || metrics.marginTop >= mToolbarHeight / 2
              || metrics.pageRectBottom == metrics.viewportRectBottom) {
            mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
        } else {
            mDynamicToolbar.setVisible(false, VisibilityTransition.ANIMATE);
        }
    }

    public void refreshToolbarHeight() {
        ThreadUtils.assertOnUiThread();

        int height = 0;
        if (mViewFlipper != null) {
            height = mViewFlipper.getHeight();
        }

        if (!mDynamicToolbar.isEnabled() || isHomePagerVisible()) {
            // Use aVisibleHeight here so that when the dynamic toolbar is
            // enabled, the padding will animate with the toolbar becoming
            // visible.
            if (mDynamicToolbar.isEnabled()) {
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
            mDynamicToolbar.setVisible(true, VisibilityTransition.IMMEDIATE);
        }
    }

    @Override
    void toggleChrome(final boolean aShow) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (aShow) {
                    mViewFlipper.setVisibility(View.VISIBLE);
                } else {
                    mViewFlipper.setVisibility(View.GONE);
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
                mViewFlipper.setVisibility(View.VISIBLE);
                mViewFlipper.requestFocusFromTouch();
            }
        });
    }

    @Override
    public void refreshChrome() {
        invalidateOptionsMenu();

        if (mTabsPanel != null) {
            updateSideBarState();
            mTabsPanel.refresh();
        }

        mBrowserToolbar.refresh();
    }

    @Override
    public boolean hasTabsSideBar() {
        return (mTabsPanel != null && mTabsPanel.isSideBar());
    }

    private void setBrowserToolbarVisible(final boolean visible) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (mDynamicToolbar.isEnabled()) {
                    mDynamicToolbar.setVisible(visible, VisibilityTransition.IMMEDIATE);
                }
            }
        });
    }

    private void updateSideBarState() {
        if (mMainLayoutAnimator != null)
            mMainLayoutAnimator.stop();

        boolean isSideBar = (HardwareUtils.isTablet() && getOrientation() == Configuration.ORIENTATION_LANDSCAPE);
        final int sidebarWidth = getResources().getDimensionPixelSize(R.dimen.tabs_sidebar_width);

        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) mTabsPanel.getLayoutParams();
        lp.width = (isSideBar ? sidebarWidth : ViewGroup.LayoutParams.MATCH_PARENT);
        mTabsPanel.requestLayout();

        final boolean sidebarIsShown = (isSideBar && mTabsPanel.isShown());
        final int mainLayoutScrollX = (sidebarIsShown ? -sidebarWidth : 0);
        mMainLayout.scrollTo(mainLayoutScrollX, 0);

        mTabsPanel.setIsSideBar(isSideBar);
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        if ("Accounts:Create".equals(event)) {
            // Do exactly the same thing as if you tapped 'Sync' in Settings.
            final Intent intent = new Intent(getContext(), FxAccountGetStartedActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            final NativeJSObject extras = message.optObject("extras", null);
            if (extras != null) {
                intent.putExtra("extras", extras.toString());
            }
            getContext().startActivity(intent);

        } else if ("CharEncoding:Data".equals(event)) {
            final NativeJSObject[] charsets = message.getObjectArray("charsets");
            final int selected = message.getInt("selected");

            final String[] titleArray = new String[charsets.length];
            final String[] codeArray = new String[charsets.length];
            for (int i = 0; i < charsets.length; i++) {
                final NativeJSObject charset = charsets[i];
                titleArray[i] = charset.getString("title");
                codeArray[i] = charset.getString("code");
            }

            final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
            dialogBuilder.setSingleChoiceItems(titleArray, selected,
                    new AlertDialog.OnClickListener() {
                @Override
                public void onClick(final DialogInterface dialog, final int which) {
                    GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createBroadcastEvent("CharEncoding:Set", codeArray[which]));
                    dialog.dismiss();
                }
            });
            dialogBuilder.setNegativeButton(R.string.button_cancel,
                    new AlertDialog.OnClickListener() {
                @Override
                public void onClick(final DialogInterface dialog, final int which) {
                    dialog.dismiss();
                }
            });
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    dialogBuilder.show();
                }
            });

        } else if ("CharEncoding:State".equals(event)) {
            final boolean visible = message.getString("visible").equals("true");
            GeckoPreferences.setCharEncodingState(visible);
            final Menu menu = mMenu;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    if (menu != null) {
                        menu.findItem(R.id.char_encoding).setVisible(visible);
                    }
                }
            });

        } else if ("Feedback:LastUrl".equals(event)) {
            getLastUrl();

        } else if ("Feedback:MaybeLater".equals(event)) {
            resetFeedbackLaunchCount();

        } else if ("Feedback:OpenPlayStore".equals(event)) {
            final Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse("market://details?id=" + getPackageName()));
            startActivity(intent);

        } else if ("Menu:Add".equals(event)) {
            final MenuItemInfo info = new MenuItemInfo();
            info.label = message.getString("name");
            info.id = message.getInt("id") + ADDON_MENU_OFFSET;
            info.icon = message.optString("icon", null);
            info.checked = message.optBoolean("checked", false);
            info.enabled = message.optBoolean("enabled", true);
            info.visible = message.optBoolean("visible", true);
            info.checkable = message.optBoolean("checkable", false);
            final int parent = message.optInt("parent", 0);
            info.parent = parent <= 0 ? parent : parent + ADDON_MENU_OFFSET;
            final MenuItemInfo menuItemInfo = info;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    addAddonMenuItem(menuItemInfo);
                }
            });

        } else if ("Menu:Remove".equals(event)) {
            final int id = message.getInt("id") + ADDON_MENU_OFFSET;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    removeAddonMenuItem(id);
                }
            });

        } else if ("Reader:ListStatusRequest".equals(event)) {
            handleReaderListStatusRequest(message.getString("url"));

        } else if ("Reader:Removed".equals(event)) {
            final String url = message.getString("url");
            handleReaderRemoved(url);

        } else if ("Reader:Share".equals(event)) {
            final String title = message.getString("title");
            final String url = message.getString("url");
            GeckoAppShell.openUriExternal(url, "text/plain", "", "", Intent.ACTION_SEND, title);

        } else if ("Settings:Show".equals(event)) {
            final String resource =
                    message.optString(GeckoPreferences.INTENT_EXTRA_RESOURCES, null);
            final Intent settingsIntent = new Intent(this, GeckoPreferences.class);
            GeckoPreferences.setResourceToOpen(settingsIntent, resource);
            startActivityForResult(settingsIntent, ACTIVITY_REQUEST_PREFERENCES);

            // Don't use a transition to settings if we're on a device where that
            // would look bad.
            if (HardwareUtils.IS_KINDLE_DEVICE) {
                overridePendingTransition(0, 0);
            }

        } else if ("Telemetry:Gather".equals(event)) {
            Telemetry.HistogramAdd("PLACES_PAGES_COUNT",
                    BrowserDB.getCount(getContentResolver(), "history"));
            Telemetry.HistogramAdd("PLACES_BOOKMARKS_COUNT",
                    BrowserDB.getCount(getContentResolver(), "bookmarks"));
            Telemetry.HistogramAdd("FENNEC_FAVICONS_COUNT",
                    BrowserDB.getCount(getContentResolver(), "favicons"));
            Telemetry.HistogramAdd("FENNEC_THUMBNAILS_COUNT",
                    BrowserDB.getCount(getContentResolver(), "thumbnails"));
            Telemetry.HistogramAdd("BROWSER_IS_USER_DEFAULT", (isDefaultBrowser() ? 1 : 0));
        } else if ("Updater:Launch".equals(event)) {
            handleUpdaterLaunch();

        } else if ("BrowserToolbar:Visibility".equals(event)) {
            setBrowserToolbarVisible(message.getBoolean("visible"));

        } else {
            super.handleMessage(event, message, callback);
        }
    }

    /**
     * Use a dummy Intent to do a default browser check.
     *
     * @return true if this package is the default browser on this device, false otherwise.
     */
    private boolean isDefaultBrowser() {
        final Intent viewIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.mozilla.org"));
        final ResolveInfo info = getPackageManager().resolveActivity(viewIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (info == null) {
            // No default is set
            return false;
        }

        final String packageName = info.activityInfo.packageName;
        return (TextUtils.equals(packageName, getPackageName()));
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Menu:Update")) {
                final int id = message.getInt("id") + ADDON_MENU_OFFSET;
                final JSONObject options = message.getJSONObject("options");
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        updateAddonMenuItem(id, options);
                    }
                });
            } else if (event.equals("Gecko:DelayedStartup")) {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Force tabs panel inflation once the initial
                        // pageload is finished.
                        ensureTabsPanelExists();
                    }
                });

                if (AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED) {
                    // Start (this acts as ping if started already) the stumbler lib; if the stumbler has queued data it will upload it.
                    // Stumbler operates on its own thread, and startup impact is further minimized by delaying work (such as upload) a few seconds.
                    GeckoPreferences.broadcastStumblerPref(this);
                }
                super.handleMessage(event, message);
            } else if (event.equals("Gecko:Ready")) {
                // Handle this message in GeckoApp, but also enable the Settings
                // menuitem, which is specific to BrowserApp.
                super.handleMessage(event, message);
                final Menu menu = mMenu;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (menu != null) {
                            menu.findItem(R.id.settings).setEnabled(true);
                            menu.findItem(R.id.help).setEnabled(true);
                        }
                    }
                });

                // Display notification for Mozilla data reporting, if data should be collected.
                if (AppConstants.MOZ_DATA_REPORTING) {
                    DataReportingNotification.checkAndNotifyPolicy(GeckoAppShell.getContext());
                }

            } else if (event.equals("Reader:Added")) {
                final int result = message.getInt("result");
                handleReaderAdded(result, messageToReadingListContentValues(message));
            } else if (event.equals("Reader:FaviconRequest")) {
                final String url = message.getString("url");
                handleReaderFaviconRequest(url);
            } else if (event.equals("Search:Keyword")) {
                storeSearchQuery(message.getString("query"));
            } else if (event.equals("Prompt:ShowTop")) {
                // Bring this activity to front so the prompt is visible..
                Intent bringToFrontIntent = new Intent();
                bringToFrontIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.BROWSER_INTENT_CLASS_NAME);
                bringToFrontIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
                startActivity(bringToFrontIntent);
            } else if (event.equals("Accounts:Exist")) {
                final String kind = message.getString("kind");
                final JSONObject response = new JSONObject();

                if ("any".equals(kind)) {
                    response.put("exists", SyncAccounts.syncAccountsExist(getContext()) ||
                                           FirefoxAccounts.firefoxAccountsExist(getContext()));
                    EventDispatcher.sendResponse(message, response);
                } else if ("fxa".equals(kind)) {
                    response.put("exists", FirefoxAccounts.firefoxAccountsExist(getContext()));
                    EventDispatcher.sendResponse(message, response);
                } else if ("sync11".equals(kind)) {
                    response.put("exists", SyncAccounts.syncAccountsExist(getContext()));
                    EventDispatcher.sendResponse(message, response);
                } else {
                    response.put("error", "Unknown kind");
                    EventDispatcher.sendError(message, response);
                }
            } else {
                super.handleMessage(event, message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    @Override
    public void addTab() {
        // Always load about:home when opening a new tab.
        Tabs.getInstance().loadUrl(AboutPages.HOME, Tabs.LOADURL_NEW_TAB);
    }

    @Override
    public void addPrivateTab() {
        Tabs.getInstance().loadUrl(AboutPages.PRIVATEBROWSING, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
    }

    @Override
    public void showNormalTabs() {
        showTabs(TabsPanel.Panel.NORMAL_TABS);
    }

    @Override
    public void showPrivateTabs() {
        showTabs(TabsPanel.Panel.PRIVATE_TABS);
    }
    /**
    * Ensure the TabsPanel view is properly inflated and returns
    * true when the view has been inflated, false otherwise.
    */
    private boolean ensureTabsPanelExists() {
        if (mTabsPanel != null) {
            return false;
        }

        ViewStub tabsPanelStub = (ViewStub) findViewById(R.id.tabs_panel);
        mTabsPanel = (TabsPanel) tabsPanelStub.inflate();

        mTabsPanel.setTabsLayoutChangeListener(this);
        updateSideBarState();

        return true;
    }

    private void showTabs(final TabsPanel.Panel panel) {
        if (Tabs.getInstance().getDisplayCount() == 0)
            return;

        if (ensureTabsPanelExists()) {
            // If we've just inflated the tabs panel, only show it once the current
            // layout pass is done to avoid displayed temporary UI states during
            // relayout.
            ViewTreeObserver vto = mTabsPanel.getViewTreeObserver();
            if (vto.isAlive()) {
                vto.addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        mTabsPanel.getViewTreeObserver().removeGlobalOnLayoutListener(this);
                        mTabsPanel.show(panel);
                    }
                });
            }
        } else {
            mTabsPanel.show(panel);
        }
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
        return (mTabsPanel != null && mTabsPanel.isShown());
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
        if (mDynamicToolbar.isEnabled()) {
            if (width > 0 && height > 0) {
                mDynamicToolbar.setPinned(true, PinReason.RELAYOUT);
                mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
            } else {
                mDynamicToolbar.setPinned(false, PinReason.RELAYOUT);
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
        mDynamicToolbar.onSaveInstanceState(outState);
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
        final Tab tab;

        if (AboutPages.isAboutReader(url)) {
            tab = tabs.getFirstReaderTabForUrl(url, tabs.getSelectedTab().isPrivate());
        } else {
            tab = tabs.getFirstTabForUrl(url, tabs.getSelectedTab().isPrivate());
        }

        if (tab == null) {
            return false;
        }

        // Set the target tab to null so it does not get selected (on editing
        // mode exit) in lieu of the tab we are about to select.
        mTargetTabForEditingMode = null;
        tabs.selectTab(tab.getId());

        mBrowserToolbar.cancelEdit();

        return true;
    }

    private void openUrlAndStopEditing(String url) {
        openUrlAndStopEditing(url, null, false);
    }

    private void openUrlAndStopEditing(String url, boolean newTab) {
        openUrlAndStopEditing(url, null, newTab);
    }

    private void openUrlAndStopEditing(String url, String searchEngine) {
        openUrlAndStopEditing(url, searchEngine, false);
    }

    private void openUrlAndStopEditing(String url, String searchEngine, boolean newTab) {
        int flags = Tabs.LOADURL_NONE;
        if (newTab) {
            flags |= Tabs.LOADURL_NEW_TAB;
            if (Tabs.getInstance().getSelectedTab().isPrivate()) {
                flags |= Tabs.LOADURL_PRIVATE;
            }
        }

        Tabs.getInstance().loadUrl(url, searchEngine, -1, flags);

        mBrowserToolbar.cancelEdit();
    }

    private boolean isHomePagerVisible() {
        return (mHomePager != null && mHomePager.isVisible()
            && mHomePagerContainer != null && mHomePagerContainer.getVisibility() == View.VISIBLE);
    }

    /* Favicon stuff. */
    private static OnFaviconLoadedListener sFaviconLoadedListener = new OnFaviconLoadedListener() {
        @Override
        public void onFaviconLoaded(String pageUrl, String faviconURL, Bitmap favicon) {
            // If we failed to load a favicon, we use the default favicon instead.
            Tabs.getInstance()
                .updateFaviconForURL(pageUrl,
                                     (favicon == null) ? Favicons.defaultFavicon : favicon);
        }
    };

    private void loadFavicon(final Tab tab) {
        maybeCancelFaviconLoad(tab);

        final int tabFaviconSize = getResources().getDimensionPixelSize(R.dimen.browser_toolbar_favicon_size);

        int flags = (tab.isPrivate() || tab.getErrorType() != Tab.ErrorType.NONE) ? 0 : LoadFaviconTask.FLAG_PERSIST;
        int id = Favicons.getSizedFavicon(tab.getURL(), tab.getFaviconURL(), tabFaviconSize, flags, sFaviconLoadedListener);

        tab.setFaviconLoadId(id);
    }

    private void maybeCancelFaviconLoad(Tab tab) {
        int faviconLoadId = tab.getFaviconLoadId();

        if (Favicons.NOT_LOADING == faviconLoadId) {
            return;
        }

        // Cancel load task and reset favicon load state if it wasn't already
        // in NOT_LOADING state.
        Favicons.cancelFaviconLoad(faviconLoadId);
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

        if (mBrowserToolbar.isEditing() || mBrowserToolbar.isAnimating()) {
            return;
        }

        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        mTargetTabForEditingMode = (selectedTab != null ? selectedTab.getId() : null);

        final PropertyAnimator animator = new PropertyAnimator(250);
        animator.setUseHardwareLayer(false);

        mBrowserToolbar.startEditing(url, animator);

        final String panelId = selectedTab.getMostRecentHomePanel();
        showHomePagerWithAnimator(panelId, animator);

        animator.start();
        Telemetry.startUISession(TelemetryContract.Session.AWESOMESCREEN);
    }

    private void commitEditingMode() {
        if (!mBrowserToolbar.isEditing()) {
            return;
        }

        Telemetry.stopUISession(TelemetryContract.Session.AWESOMESCREEN,
                                TelemetryContract.Reason.COMMIT);

        final String url = mBrowserToolbar.commitEdit();

        // HACK: We don't know the url that will be loaded when hideHomePager is initially called
        // in BrowserToolbar's onStopEditing listener so on the awesomescreen, hideHomePager will
        // use the url "about:home" and return without taking any action. hideBrowserSearch is
        // then called, but since hideHomePager changes both HomePager and LayerView visibility
        // and exited without taking an action, no Views are displayed and graphical corruption is
        // visible instead.
        //
        // Here we call hideHomePager for the second time with the URL to be loaded so that
        // hideHomePager is called with the correct state for the upcoming page load.
        //
        // Expected to be fixed by bug 915825.
        hideHomePager(url);

        // Don't do anything if the user entered an empty URL.
        if (TextUtils.isEmpty(url)) {
            return;
        }

        // If the URL doesn't look like a search query, just load it.
        if (!StringUtils.isSearchQuery(url, true)) {
            Tabs.getInstance().loadUrl(url, Tabs.LOADURL_USER_ENTERED);
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.ACTIONBAR, "user");
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

                // If there isn't a bookmark keyword, load the url. This may result in a query
                // using the default search engine.
                if (TextUtils.isEmpty(keywordUrl)) {
                    Tabs.getInstance().loadUrl(url, Tabs.LOADURL_USER_ENTERED);
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.ACTIONBAR, "user");
                    return;
                }

                recordSearch(null, "barkeyword");

                // Otherwise, construct a search query from the bookmark keyword.
                final String searchUrl = keywordUrl.replace("%s", URLEncoder.encode(keywordSearch));
                Tabs.getInstance().loadUrl(searchUrl, Tabs.LOADURL_USER_ENTERED);
                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL,
                                      TelemetryContract.Method.ACTIONBAR,
                                      "keyword");
            }
        });
    }

    /**
     * Record in Health Report that a search has occurred.
     *
     * @param engine
     *        a search engine instance. Can be null.
     * @param where
     *        where the search was initialized; one of the values in
     *        {@link BrowserHealthRecorder#SEARCH_LOCATIONS}.
     */
    private static void recordSearch(SearchEngine engine, String where) {
        Log.i(LOGTAG, "Recording search: " +
                      ((engine == null) ? "null" : engine.name) +
                      ", " + where);
        try {
            String identifier = (engine == null) ? "other" : engine.getEngineIdentifier();
            JSONObject message = new JSONObject();
            message.put("type", BrowserHealthRecorder.EVENT_SEARCH);
            message.put("location", where);
            message.put("identifier", identifier);
            EventDispatcher.getInstance().dispatchEvent(message, null);
        } catch (Exception e) {
            Log.w(LOGTAG, "Error recording search.", e);
        }
    }

    /**
     * Store search query in SearchHistoryProvider.
     *
     * @param query
     *        a search query to store. We won't store empty queries.
     */
    private void storeSearchQuery(String query) {
        if (TextUtils.isEmpty(query)) {
            return;
        }

        final ContentValues values = new ContentValues();
        values.put(SearchHistory.QUERY, query);
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                getContentResolver().insert(SearchHistory.CONTENT_URI, values);
            }
        });
    }

    void filterEditingMode(String searchTerm, AutocompleteHandler handler) {
        if (TextUtils.isEmpty(searchTerm)) {
            hideBrowserSearch();
        } else {
            showBrowserSearch();
            mBrowserSearch.filter(searchTerm, handler);
        }
    }

    /**
     * Selects the target tab for editing mode. This is expected to be the tab selected on editing
     * mode entry, unless it is subsequently overridden.
     *
     * A background tab may be selected while editing mode is active (e.g. popups), causing the
     * new url to load in the newly selected tab. Call this method on editing mode exit to
     * mitigate this.
     */
    private void selectTargetTabForEditingMode() {
        if (mTargetTabForEditingMode != null) {
            Tabs.getInstance().selectTab(mTargetTabForEditingMode);
        }

        mTargetTabForEditingMode = null;
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
            String panelId = AboutPages.getPanelIdFromAboutHomeUrl(tab.getURL());
            if (panelId == null) {
                // No panel was specified in the URL. Try loading the most recent
                // home panel for this tab.
                panelId = tab.getMostRecentHomePanel();
            }
            showHomePager(panelId);

            if (mDynamicToolbar.isEnabled()) {
                mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
            }
        } else {
            hideHomePager();
        }
    }

    @Override
    public void onLocaleReady(final String locale) {
        Log.d(LOGTAG, "onLocaleReady: " + locale);
        super.onLocaleReady(locale);

        HomePanelsManager.getInstance().onLocaleReady(locale);

        if (mMenu != null) {
            mMenu.clear();
            onCreateOptionsMenu(mMenu);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(LOGTAG, "onActivityResult: " + requestCode + ", " + resultCode + ", " + data);
        switch (requestCode) {
            case ACTIVITY_REQUEST_PREFERENCES:
                // We just returned from preferences. If our locale changed,
                // we need to redisplay at this point, and do any other browser-level
                // bookkeeping that we associate with a locale change.
                if (resultCode != GeckoPreferences.RESULT_CODE_LOCALE_DID_CHANGE) {
                    Log.d(LOGTAG, "No locale change returning from preferences; nothing to do.");
                    return;
                }

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final LocaleManager localeManager = BrowserLocaleManager.getInstance();
                        final Locale locale = localeManager.getCurrentLocale(getApplicationContext());
                        Log.d(LOGTAG, "Read persisted locale " + locale);
                        if (locale == null) {
                            return;
                        }
                        onLocaleChanged(BrowserLocaleManager.getLanguageTag(locale));
                    }
                });
                break;
            default:
                super.onActivityResult(requestCode, resultCode, data);
        }
    }

    private void showHomePager(String panelId) {
        showHomePagerWithAnimator(panelId, null);
    }

    private void showHomePagerWithAnimator(String panelId, PropertyAnimator animator) {
        if (isHomePagerVisible()) {
            // Home pager already visible, make sure it shows the correct panel.
            mHomePager.showPanel(panelId);
            return;
        }

        // Refresh toolbar height to possibly restore the toolbar padding
        refreshToolbarHeight();

        // Show the toolbar before hiding about:home so the
        // onMetricsChanged callback still works.
        if (mDynamicToolbar.isEnabled()) {
            mDynamicToolbar.setVisible(true, VisibilityTransition.IMMEDIATE);
        }

        if (mHomePager == null) {
            final ViewStub homePagerStub = (ViewStub) findViewById(R.id.home_pager_stub);
            mHomePager = (HomePager) homePagerStub.inflate();

            mHomePager.setOnPanelChangeListener(new HomePager.OnPanelChangeListener() {
                @Override
                public void onPanelSelected(String panelId) {
                    final Tab currentTab = Tabs.getInstance().getSelectedTab();
                    if (currentTab != null) {
                        currentTab.setMostRecentHomePanel(panelId);
                    }
                }
            });

            // Don't show the banner in guest mode.
            if (!getProfile().inGuestMode()) {
                final ViewStub homeBannerStub = (ViewStub) findViewById(R.id.home_banner_stub);
                final HomeBanner homeBanner = (HomeBanner) homeBannerStub.inflate();
                mHomePager.setBanner(homeBanner);

                // Remove the banner from the view hierarchy if it is dismissed.
                homeBanner.setOnDismissListener(new HomeBanner.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        mHomePager.setBanner(null);
                        mHomePagerContainer.removeView(homeBanner);
                    }
                });
            }
        }

        mHomePagerContainer.setVisibility(View.VISIBLE);
        mHomePager.load(getSupportLoaderManager(),
                        getSupportFragmentManager(),
                        panelId, animator);

        // Hide the web content so it cannot be focused by screen readers.
        hideWebContentOnPropertyAnimationEnd(animator);
    }

    private void hideWebContentOnPropertyAnimationEnd(final PropertyAnimator animator) {
        if (animator == null) {
            hideWebContent();
            return;
        }

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                mHideWebContentOnAnimationEnd = true;
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (mHideWebContentOnAnimationEnd) {
                    hideWebContent();
                }
            }
        });
    }

    private void hideWebContent() {
        // The view is set to INVISIBLE, rather than GONE, to avoid
        // the additional requestLayout() call.
        mLayerView.setVisibility(View.INVISIBLE);
    }

    /**
     * Hides the HomePager, using the url of the currently selected tab as the url to be
     * loaded.
     */
    private void hideHomePager() {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        final String url = (selectedTab != null) ? selectedTab.getURL() : null;

        hideHomePager(url);
    }

    /**
     * Hides the HomePager. The given url should be the url of the page to be loaded, or null
     * if a new page is not being loaded.
     */
    private void hideHomePager(final String url) {
        if (!isHomePagerVisible() || AboutPages.isAboutHome(url)) {
            return;
        }

        // Prevent race in hiding web content - see declaration for more info.
        mHideWebContentOnAnimationEnd = false;

        // Display the previously hidden web content (which prevented screen reader access).
        mLayerView.setVisibility(View.VISIBLE);
        mHomePagerContainer.setVisibility(View.GONE);

        if (mHomePager != null) {
            mHomePager.unload();
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

        // Prevent overdraw by hiding the underlying HomePager View.
        mHomePager.setVisibility(View.INVISIBLE);

        final FragmentManager fm = getSupportFragmentManager();

        // In certain situations, showBrowserSearch() can be called immediately after hideBrowserSearch()
        // (see bug 925012). Because of an Android bug (http://code.google.com/p/android/issues/detail?id=61179),
        // calling FragmentTransaction#add immediately after FragmentTransaction#remove won't add the fragment's
        // view to the layout. Calling FragmentManager#executePendingTransactions before re-adding the fragment
        // prevents this issue.
        fm.executePendingTransactions();

        fm.beginTransaction().add(R.id.search_container, mBrowserSearch, BROWSER_SEARCH_TAG).commitAllowingStateLoss();
        mBrowserSearch.setUserVisibleHint(true);
    }

    private void hideBrowserSearch() {
        if (!mBrowserSearch.getUserVisibleHint()) {
            return;
        }

        // To prevent overdraw, the HomePager is hidden when BrowserSearch is displayed:
        // reverse that.
        mHomePager.setVisibility(View.VISIBLE);

        mBrowserSearchContainer.setVisibility(View.INVISIBLE);

        getSupportFragmentManager().beginTransaction()
                .remove(mBrowserSearch).commitAllowingStateLoss();
        mBrowserSearch.setUserVisibleHint(false);
    }

    /**
     * Hides certain UI elements (e.g. button toast, tabs tray) when the
     * user touches the main layout.
     */
    private class HideOnTouchListener implements TouchEventInterceptor {
        private boolean mIsHidingTabs;
        private final Rect mTempRect = new Rect();

        @Override
        public boolean onInterceptTouchEvent(View view, MotionEvent event) {
            // Only try to hide the button toast if it's already inflated.
            if (mToast != null) {
                mToast.hide(false, ButtonToast.ReasonHidden.TOUCH_OUTSIDE);
            }

            // We need to account for scroll state for the touched view otherwise
            // tapping on an "empty" part of the view will still be considered a
            // valid touch event.
            if (view.getScrollX() != 0 || view.getScrollY() != 0) {
                view.getHitRect(mTempRect);
                mTempRect.offset(-view.getScrollX(), -view.getScrollY());

                int[] viewCoords = new int[2];
                view.getLocationOnScreen(viewCoords);

                int x = (int) event.getRawX() - viewCoords[0];
                int y = (int) event.getRawY() - viewCoords[1];

                if (!mTempRect.contains(x, y))
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

    private static Menu findParentMenu(Menu menu, MenuItem item) {
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

    /**
     * Add the provided item to the provided menu, which should be
     * the root (mMenu).
     */
    private void addAddonMenuItemToMenu(final Menu menu, final MenuItemInfo info) {
        info.added = true;

        final Menu destination;
        if (info.parent == 0) {
            destination = menu;
        } else if (info.parent == GECKO_TOOLS_MENU) {
            MenuItem tools = menu.findItem(R.id.tools);
            destination = tools != null ? tools.getSubMenu() : menu;
        } else {
            MenuItem parent = menu.findItem(info.parent);
            if (parent == null) {
                return;
            }

            Menu parentMenu = findParentMenu(menu, parent);

            if (!parent.hasSubMenu()) {
                parentMenu.removeItem(parent.getItemId());
                destination = parentMenu.addSubMenu(Menu.NONE, parent.getItemId(), Menu.NONE, parent.getTitle());
                if (parent.getIcon() != null) {
                    ((SubMenu) destination).getItem().setIcon(parent.getIcon());
                }
            } else {
                destination = parent.getSubMenu();
            }
        }

        MenuItem item = destination.add(Menu.NONE, info.id, Menu.NONE, info.label);

        item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                Log.i(LOGTAG, "Menu item clicked");
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Menu:Clicked", Integer.toString(info.id - ADDON_MENU_OFFSET)));
                return true;
            }
        });

        if (info.icon == null) {
            item.setIcon(R.drawable.ic_menu_addons_filler);
        } else {
            final int id = info.id;
            BitmapUtils.getDrawable(this, info.icon, new BitmapUtils.BitmapLoader() {
                @Override
                public void onBitmapFound(Drawable d) {
                    // TODO: why do we re-find the item?
                    MenuItem item = destination.findItem(id);
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
        }

        item.setCheckable(info.checkable);
        item.setChecked(info.checked);
        item.setEnabled(info.enabled);
        item.setVisible(info.visible);
    }

    private void addAddonMenuItem(final MenuItemInfo info) {
        if (mAddonMenuItemsCache == null) {
            mAddonMenuItemsCache = new Vector<MenuItemInfo>();
        }

        // Mark it as added if the menu was ready.
        info.added = (mMenu != null);

        // Always cache so we can rebuild after a locale switch.
        mAddonMenuItemsCache.add(info);

        if (mMenu == null) {
            return;
        }

        addAddonMenuItemToMenu(mMenu, info);
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
                    item.label = options.optString("name", item.label);
                    item.checkable = options.optBoolean("checkable", item.checkable);
                    item.checked = options.optBoolean("checked", item.checked);
                    item.enabled = options.optBoolean("enabled", item.enabled);
                    item.visible = options.optBoolean("visible", item.visible);
                    item.added = (mMenu != null);
                    break;
                }
            }
        }

        if (mMenu == null) {
            return;
        }

        MenuItem menuItem = mMenu.findItem(id);
        if (menuItem != null) {
            menuItem.setTitle(options.optString("name", menuItem.getTitle().toString()));
            menuItem.setCheckable(options.optBoolean("checkable", menuItem.isCheckable()));
            menuItem.setChecked(options.optBoolean("checked", menuItem.isChecked()));
            menuItem.setEnabled(options.optBoolean("enabled", menuItem.isEnabled()));
            menuItem.setVisible(options.optBoolean("visible", menuItem.isVisible()));
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Sets mMenu = menu.
        super.onCreateOptionsMenu(menu);

        // Inform the menu about the action-items bar.
        if (menu instanceof GeckoMenu &&
            HardwareUtils.isTablet()) {
            ((GeckoMenu) menu).setActionItemBarPresenter(mBrowserToolbar);
        }

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.browser_app_menu, mMenu);

        // Add add-on menu items, if any exist.
        if (mAddonMenuItemsCache != null && !mAddonMenuItemsCache.isEmpty()) {
            for (MenuItemInfo item : mAddonMenuItemsCache) {
                addAddonMenuItemToMenu(mMenu, item);
            }
        }

        // Action providers are available only ICS+.
        if (Versions.feature14Plus) {
            GeckoMenuItem share = (GeckoMenuItem) mMenu.findItem(R.id.share);
            GeckoActionProvider provider = GeckoActionProvider.getForType(GeckoActionProvider.DEFAULT_MIME_TYPE, this);
            share.setActionProvider(provider);
        }

        return true;
    }

    @Override
    public void openOptionsMenu() {
        if (areTabsShown()) {
            mTabsPanel.showMenu();
            return;
        }

        // Scroll custom menu to the top
        if (mMenuPanel != null)
            mMenuPanel.scrollTo(0, 0);

        if (!mBrowserToolbar.openOptionsMenu())
            super.openOptionsMenu();

        if (mDynamicToolbar.isEnabled()) {
            mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
        }
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
                    mViewFlipper.setVisibility(View.GONE);
                    if (mDynamicToolbar.isEnabled()) {
                        mDynamicToolbar.setVisible(false, VisibilityTransition.IMMEDIATE);
                        mLayerView.getLayerMarginsAnimator().setMaxMargins(0, 0, 0, 0);
                    } else {
                        setToolbarMargin(0);
                    }
                } else {
                    mViewFlipper.setVisibility(View.VISIBLE);
                    if (mDynamicToolbar.isEnabled()) {
                        mDynamicToolbar.setVisible(true, VisibilityTransition.IMMEDIATE);
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

        if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            aMenu.findItem(R.id.settings).setEnabled(false);
            aMenu.findItem(R.id.help).setEnabled(false);
        }

        Tab tab = Tabs.getInstance().getSelectedTab();
        MenuItem bookmark = aMenu.findItem(R.id.bookmark);
        MenuItem back = aMenu.findItem(R.id.back);
        MenuItem forward = aMenu.findItem(R.id.forward);
        MenuItem share = aMenu.findItem(R.id.share);
        MenuItem saveAsPDF = aMenu.findItem(R.id.save_as_pdf);
        MenuItem charEncoding = aMenu.findItem(R.id.char_encoding);
        MenuItem findInPage = aMenu.findItem(R.id.find_in_page);
        MenuItem desktopMode = aMenu.findItem(R.id.desktop_mode);
        MenuItem enterGuestMode = aMenu.findItem(R.id.new_guest_session);
        MenuItem exitGuestMode = aMenu.findItem(R.id.exit_guest_session);

        // Only show the "Quit" menu item on pre-ICS, television devices,
        // or if the user has explicitly enabled the clear on shutdown pref.
        // (We check the pref last to save the pref read.)
        // In ICS+, it's easy to kill an app through the task switcher.
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(this);
        final boolean visible = Versions.preICS ||
                                HardwareUtils.isTelevision() ||
                                !PrefUtils.getStringSet(GeckoSharedPrefs.forProfile(this),
                                                        ClearOnShutdownPref.PREF,
                                                        new HashSet<String>()).isEmpty();
        aMenu.findItem(R.id.quit).setVisible(visible);

        if (tab == null || tab.getURL() == null) {
            bookmark.setEnabled(false);
            back.setEnabled(false);
            forward.setEnabled(false);
            share.setEnabled(false);
            saveAsPDF.setEnabled(false);
            findInPage.setEnabled(false);

            // NOTE: Use MenuUtils.safeSetEnabled because some actions might
            // be on the BrowserToolbar context menu
            MenuUtils.safeSetEnabled(aMenu, R.id.page, false);
            MenuUtils.safeSetEnabled(aMenu, R.id.subscribe, false);
            MenuUtils.safeSetEnabled(aMenu, R.id.add_search_engine, false);
            MenuUtils.safeSetEnabled(aMenu, R.id.site_settings, false);
            MenuUtils.safeSetEnabled(aMenu, R.id.add_to_launcher, false);

            return true;
        }

        bookmark.setEnabled(!AboutPages.isAboutReader(tab.getURL()));
        bookmark.setVisible(!GeckoProfile.get(this).inGuestMode());
        bookmark.setCheckable(true);
        bookmark.setChecked(tab.isBookmark());
        bookmark.setIcon(tab.isBookmark() ? R.drawable.ic_menu_bookmark_remove : R.drawable.ic_menu_bookmark_add);

        back.setEnabled(tab.canDoBack());
        forward.setEnabled(tab.canDoForward());
        desktopMode.setChecked(tab.getDesktopMode());
        desktopMode.setIcon(tab.getDesktopMode() ? R.drawable.ic_menu_desktop_mode_on : R.drawable.ic_menu_desktop_mode_off);

        String url = tab.getURL();
        if (AboutPages.isAboutReader(url)) {
            String urlFromReader = ReaderModeUtils.getUrlFromAboutReader(url);
            if (urlFromReader != null) {
                url = urlFromReader;
            }
        }

        // Disable share menuitem for about:, chrome:, file:, and resource: URIs
        final boolean inGuestMode = GeckoProfile.get(this).inGuestMode();
        share.setVisible(!inGuestMode);
        share.setEnabled(StringUtils.isShareableUrl(url) && !inGuestMode);
        MenuUtils.safeSetEnabled(aMenu, R.id.apps, !inGuestMode);
        MenuUtils.safeSetEnabled(aMenu, R.id.addons, !inGuestMode);
        MenuUtils.safeSetEnabled(aMenu, R.id.downloads, !inGuestMode);

        // NOTE: Use MenuUtils.safeSetEnabled because some actions might
        // be on the BrowserToolbar context menu
        MenuUtils.safeSetEnabled(aMenu, R.id.page, !isAboutHome(tab));
        MenuUtils.safeSetEnabled(aMenu, R.id.subscribe, tab.hasFeeds());
        MenuUtils.safeSetEnabled(aMenu, R.id.add_search_engine, tab.hasOpenSearch());
        MenuUtils.safeSetEnabled(aMenu, R.id.site_settings, !isAboutHome(tab));
        MenuUtils.safeSetEnabled(aMenu, R.id.add_to_launcher, !isAboutHome(tab));

        // Action providers are available only ICS+.
        if (Versions.feature14Plus) {
            final GeckoActionProvider provider = ((GeckoMenuItem) share).getGeckoActionProvider();
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

        // Disable save as PDF for about:home and xul pages.
        saveAsPDF.setEnabled(!(isAboutHome(tab) ||
                               tab.getContentType().equals("application/vnd.mozilla.xul+xml") ||
                               tab.getContentType().startsWith("video/")));

        // Disable find in page for about:home, since it won't work on Java content.
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

        // Track the menu action. We don't know much about the context, but we can use this to determine
        // the frequency of use for various actions.
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, getResources().getResourceEntryName(itemId));

        if (itemId == R.id.bookmark) {
            tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                if (item.isChecked()) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.UNSAVE, TelemetryContract.Method.MENU, "bookmark");
                    tab.removeBookmark();
                    Toast.makeText(this, R.string.bookmark_removed, Toast.LENGTH_SHORT).show();
                    item.setIcon(R.drawable.ic_menu_bookmark_add);
                } else {
                    Telemetry.sendUIEvent(TelemetryContract.Event.SAVE, TelemetryContract.Method.MENU, "bookmark");
                    tab.addBookmark();
                    getButtonToast().show(false,
                        getResources().getString(R.string.bookmark_added),
                        ButtonToast.LENGTH_SHORT,
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

        if (itemId == R.id.back) {
            tab = Tabs.getInstance().getSelectedTab();
            if (tab != null)
                tab.doBack();
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

            // We want to know when the Settings activity returns, because
            // we might need to redisplay based on a locale change.
            startActivityForResult(intent, ACTIVITY_REQUEST_PREFERENCES);
            return true;
        }

        if (itemId == R.id.help) {
            final String VERSION = AppConstants.MOZ_APP_VERSION;
            final String OS = AppConstants.OS_TARGET;
            final String LOCALE = BrowserLocaleManager.getLanguageTag(Locale.getDefault());

            final String URL = getResources().getString(R.string.help_link, VERSION, OS, LOCALE);
            Tabs.getInstance().loadUrlInTab(URL);
            return true;
        }

        if (itemId == R.id.addons) {
            Tabs.getInstance().loadUrlInTab(AboutPages.ADDONS);
            return true;
        }

        if (itemId == R.id.apps) {
            Tabs.getInstance().loadUrlInTab(AboutPages.APPS);
            return true;
        }

        if (itemId == R.id.downloads) {
            Tabs.getInstance().loadUrlInTab(AboutPages.DOWNLOADS);
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

        // We have a few menu items that can also be in the context menu. If
        // we have not already handled the item, give the context menu handler
        // a chance.
        if (onContextItemSelected(item)) {
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
                        GeckoAppShell.systemExit();
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

        ps.show(res.getString(titleString), res.getString(msgString), null, ListView.CHOICE_MODE_NONE);
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
        String action = intent.getAction();

        final boolean isViewAction = Intent.ACTION_VIEW.equals(action);
        final boolean isBookmarkAction = GeckoApp.ACTION_HOMESCREEN_SHORTCUT.equals(action);

        if (mInitialized && (isViewAction || isBookmarkAction)) {
            // Dismiss editing mode if the user is loading a URL from an external app.
            mBrowserToolbar.cancelEdit();

            // GeckoApp.ACTION_HOMESCREEN_SHORTCUT means we're opening a bookmark that
            // was added to Android's homescreen.
            final TelemetryContract.Method method =
                (isViewAction ? TelemetryContract.Method.INTENT : TelemetryContract.Method.HOMESCREEN);

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, method);
        }

        super.onNewIntent(intent);

        if (AppConstants.MOZ_ANDROID_BEAM && NfcAdapter.ACTION_NDEF_DISCOVERED.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        }

        if (!mInitialized) {
            return;
        }

        // Only solicit feedback when the app has been launched from the icon shortcut.
        if (!Intent.ACTION_MAIN.equals(action)) {
            return;
        }

        (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized Boolean doInBackground() {
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
        (new UIAsyncTask.WithoutParams<String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public synchronized String doInBackground() {
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
    public void onNewTabs(List<String> urls) {
        final EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB);

        for (String url : urls) {
            if (!maybeSwitchToTab(url, flags)) {
                openUrlAndStopEditing(url, true);
            }
        }
    }

    // HomePager.OnUrlOpenListener
    @Override
    public void onUrlOpen(String url, EnumSet<OnUrlOpenListener.Flags> flags) {
        if (flags.contains(OnUrlOpenListener.Flags.OPEN_WITH_INTENT)) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(url));
            startActivity(intent);
        } else if (!maybeSwitchToTab(url, flags)) {
            openUrlAndStopEditing(url);
        }
    }

    // BrowserSearch.OnSearchListener
    @Override
    public void onSearch(SearchEngine engine, String text) {
        // Don't store searches that happen in private tabs. This assumes the user can only
        // perform a search inside the currently selected tab, which is true for searches
        // that come from SearchEngineRow.
        if (!Tabs.getInstance().getSelectedTab().isPrivate()) {
            storeSearchQuery(text);
        }
        recordSearch(engine, "barsuggest");
        openUrlAndStopEditing(text, engine.name);
    }

    // BrowserSearch.OnEditSuggestionListener
    @Override
    public void onEditSuggestion(String suggestion) {
        mBrowserToolbar.onEditSuggestion(suggestion);
    }

    @Override
    public int getLayout() { return R.layout.gecko_app; }

    @Override
    protected String getDefaultProfileName() throws NoMozillaDirectoryException {
        return GeckoProfile.getDefaultProfileName(this);
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
            Tabs.getInstance().loadUrlInTab(AboutPages.UPDATER);
            return true;
        }

        Log.w(LOGTAG, "No candidate updater found; ignoring launch request.");
        return false;
    }

    /* Implementing ActionModeCompat.Presenter */
    @Override
    public void startActionModeCompat(final ActionModeCompat.Callback callback) {
        // If actionMode is null, we're not currently showing one. Flip to the action mode view
        if (mActionMode == null) {
            mViewFlipper.showNext();
            LayerMarginsAnimator margins = mLayerView.getLayerMarginsAnimator();

            // If the toolbar is dynamic and not currently showing, just slide it in
            if (mDynamicToolbar.isEnabled() && !margins.areMarginsShown()) {
                margins.setMaxMargins(0, mViewFlipper.getHeight(), 0, 0);
                mDynamicToolbar.setVisible(true, VisibilityTransition.ANIMATE);
                mShowActionModeEndAnimation = true;
            } else {
                // Otherwise, we animate the actionbar itself
                mActionBar.animateIn();
            }

            mDynamicToolbar.setPinned(true, PinReason.ACTION_MODE);
        } else {
            // Otherwise, we're already showing an action mode. Just finish it and show the new one
            mActionMode.finish();
        }

        mActionMode = new ActionModeCompat(BrowserApp.this, callback, mActionBar);
        if (callback.onCreateActionMode(mActionMode, mActionMode.getMenu())) {
            mActionMode.invalidate();
        }
    }

    /* Implementing ActionModeCompat.Presenter */
    @Override
    public void endActionModeCompat() {
        if (mActionMode == null) {
            return;
        }

        mActionMode.finish();
        mActionMode = null;
        mDynamicToolbar.setPinned(false, PinReason.ACTION_MODE);

        mViewFlipper.showPrevious();

        // Only slide the urlbar out if it was hidden when the action mode started
        // Don't animate hiding it so that there's no flash as we switch back to url mode
        if (mShowActionModeEndAnimation) {
            mDynamicToolbar.setVisible(false, VisibilityTransition.IMMEDIATE);
            mShowActionModeEndAnimation = false;
        }
    }

    @Override
    protected HealthRecorder createHealthRecorder(final Context context,
                                                  final String profilePath,
                                                  final EventDispatcher dispatcher,
                                                  final String osLocale,
                                                  final String appLocale,
                                                  final SessionInformation previousSession) {
        return new BrowserHealthRecorder(context,
                                         GeckoSharedPrefs.forApp(context),
                                         profilePath,
                                         dispatcher,
                                         osLocale,
                                         appLocale,
                                         previousSession);
    }
}
