/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

import android.content.SharedPreferences;
import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.distribution.PartnerBrowserCustomizationsClient;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.notifications.WhatsNewReceiver;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.JavaUtil;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.webapps.WebAppManifest;
import org.mozilla.geckoview.GeckoView;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.OnAccountsUpdateListener;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.sqlite.SQLiteException;
import android.graphics.Color;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.annotation.UiThread;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;
import android.util.Log;


public class Tabs implements BundleEventListener {
    private static final String LOGTAG = "GeckoTabs";

    public static final String INTENT_EXTRA_TAB_ID = "TabId";
    public static final String INTENT_EXTRA_SESSION_UUID = "SessionUUID";
    private static final String PRIVATE_TAB_INTENT_EXTRA = "private_tab";

    // mOrder and mTabs are always of the same cardinality, and contain the same values.
    private volatile CopyOnWriteArrayList<Tab> mOrder = new CopyOnWriteArrayList<Tab>();

    // A cache that maps a tab ID to an mOrder tab position.  All access should be synchronized.
    private final TabPositionCache tabPositionCache = new TabPositionCache();

    // All writes to mSelectedTab must be synchronized on the Tabs instance.
    // In general, it's preferred to always use selectTab()).
    private volatile Tab mSelectedTab;
    private volatile int mPreviouslySelectedTabId = INVALID_TAB_ID;

    // All accesses to mTabs must be synchronized on the Tabs instance.
    private final HashMap<Integer, Tab> mTabs = new HashMap<Integer, Tab>();

    private AccountManager mAccountManager;
    private OnAccountsUpdateListener mAccountListener;

    public static final int LOADURL_NONE         = 0;
    public static final int LOADURL_NEW_TAB      = 1 << 0;
    public static final int LOADURL_USER_ENTERED = 1 << 1;
    public static final int LOADURL_PRIVATE      = 1 << 2;
    public static final int LOADURL_PINNED       = 1 << 3;
    public static final int LOADURL_DELAY_LOAD   = 1 << 4;
    public static final int LOADURL_DESKTOP      = 1 << 5;
    public static final int LOADURL_BACKGROUND   = 1 << 6;
    /** Indicates the url has been specified by a source external to the app. */
    public static final int LOADURL_EXTERNAL     = 1 << 7;
    /** Indicates the tab is the first shown after Firefox is hidden and restored. */
    public static final int LOADURL_FIRST_AFTER_ACTIVITY_UNHIDDEN = 1 << 8;
    /** Indicates that we should enter editing mode after opening the tab. */
    public static final int LOADURL_START_EDITING = 1 << 9;

    private static final long PERSIST_TABS_AFTER_MILLISECONDS = 1000 * 2;

    public static final int INVALID_TAB_ID = -1;
    // Used to indicate a new tab should be appended to the current tabs.
    public static final int NEW_LAST_INDEX = -1;

    // Bug 1410749: Desktop numbers tabs starting from 1, and various Webextension bits have
    // inherited that assumption and treat 0 as an invalid tab.
    private static final AtomicInteger sTabId = new AtomicInteger(1);
    private volatile boolean mInitialTabsAdded;

    private Context mAppContext;
    private EventDispatcher mEventDispatcher;
    private GeckoView mGeckoView;
    private ContentObserver mBookmarksContentObserver;
    private PersistTabsRunnable mPersistTabsRunnable;
    private int mPrivateClearColor;

    // Close all tabs including normal and private tabs.
    @RobocopTarget
    public void closeAllTabs() {
        for (final Tab tab : mOrder) {
            this.closeTab(tab, false);
        }
    }

    // In the normal panel we want to close all tabs (both private and normal),
    // but in the private panel we only want to close private tabs.
    public void closeAllPrivateTabs() {
        for (final Tab tab : mOrder) {
            if (tab.isPrivate()) {
                this.closeTab(tab, false);
            }
        }
    }

    private static class PersistTabsRunnable implements Runnable {
        private final BrowserDB db;
        private final Context context;
        private final Iterable<Tab> tabs;

        public PersistTabsRunnable(final Context context, Iterable<Tab> tabsInOrder) {
            this.context = context;
            this.db = BrowserDB.from(context);
            this.tabs = tabsInOrder;
        }

        @Override
        public void run() {
            try {
                db.getTabsAccessor().persistLocalTabs(context.getContentResolver(), tabs);
            } catch (SQLiteException e) {
                Log.w(LOGTAG, "Error persisting local tabs", e);
            }
        }
    };

    private Tabs() {
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Tab:GetNextTabId",
            null);

        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Content:LocationChange",
            "Content:SubframeNavigation",
            "Content:SecurityChange",
            "Content:ContentBlockingEvent",
            "Content:StateChange",
            "Content:LoadError",
            "Content:DOMContentLoaded",
            "Content:PageShow",
            "Content:DOMTitleChanged",
            "DesktopMode:Changed",
            "Link:Favicon",
            "Link:Feed",
            "Link:Manifest",
            "Link:OpenSearch",
            "Link:Touchicon",
            "Tab:Added",
            "Tab:AudioPlayingChange",
            "Tab:Close",
            "Tab:MediaPlaybackChange",
            "Tab:RecordingChange",
            "Tab:Select",
            "Tab:SetParentId",
            null);

        EventDispatcher.getInstance().registerBackgroundThreadListener(this,
            // BrowserApp already wants this on the background thread.
            "Sanitize:ClearHistory",
            null);

        mPrivateClearColor = Color.RED;
    }

    public synchronized void attachToContext(Context context, GeckoView geckoView,
                                             EventDispatcher eventDispatcher) {
        final Context appContext = context.getApplicationContext();

        mAppContext = appContext;
        mEventDispatcher = eventDispatcher;
        mGeckoView = geckoView;
        mPrivateClearColor = ContextCompat.getColor(context, R.color.tabs_tray_grey_pressed);
        mAccountManager = AccountManager.get(appContext);

        mAccountListener = new OnAccountsUpdateListener() {
            @Override
            public void onAccountsUpdated(Account[] accounts) {
                queuePersistAllTabs();
            }
        };

        // The listener will run on the background thread (see 2nd argument).
        mAccountManager.addOnAccountsUpdatedListener(mAccountListener, ThreadUtils.getBackgroundHandler(), false);

        if (mBookmarksContentObserver != null) {
            // It's safe to use the db here since we aren't doing any I/O.
            final GeckoProfile profile = GeckoProfile.get(context);
            BrowserDB.from(profile).registerBookmarkObserver(getContentResolver(), mBookmarksContentObserver);
        }
    }

    public void detachFromContext() {
        mGeckoView = null;
    }

    /**
     * Gets the tab count corresponding to the private state of the selected tab.
     *
     * If the selected tab is a non-private tab, this will return the number of
     * non-private tabs; likewise, if this is a private tab, this will return
     * the number of private tabs.
     *
     * @return the number of tabs in the current private state
     */
    public synchronized int getDisplayCount() {
        // Once mSelectedTab is non-null, it cannot be null for the remainder
        // of the object's lifetime.
        boolean getPrivate = mSelectedTab != null && mSelectedTab.isPrivate();
        int count = 0;
        for (Tab tab : mOrder) {
            if (tab.isPrivate() == getPrivate) {
                count++;
            }
        }
        return count;
    }

    public int isOpen(String url) {
        for (Tab tab : mOrder) {
            if (tab.getURL().equals(url)) {
                return tab.getId();
            }
        }
        return INVALID_TAB_ID;
    }

    // Must be synchronized to avoid racing on mBookmarksContentObserver.
    private void lazyRegisterBookmarkObserver() {
        if (mBookmarksContentObserver == null) {
            mBookmarksContentObserver = new ContentObserver(null) {
                @Override
                public void onChange(boolean selfChange) {
                    for (Tab tab : mOrder) {
                        tab.updateBookmark();
                    }
                }
            };

            // It's safe to use the db here since we aren't doing any I/O.
            final GeckoProfile profile = GeckoProfile.get(mAppContext);
            BrowserDB.from(profile).registerBookmarkObserver(getContentResolver(), mBookmarksContentObserver);
        }
    }

    private Tab addTab(int id, String url, boolean external, int parentId, String title, boolean isPrivate, int tabIndex) {
        final Tab tab = isPrivate ? new PrivateTab(mAppContext, id, url, external, parentId, title) :
                                    new Tab(mAppContext, id, url, external, parentId, title);
        synchronized (this) {
            lazyRegisterBookmarkObserver();
            mTabs.put(id, tab);

            if (tabIndex > -1) {
                mOrder.add(tabIndex, tab);
                if (tabPositionCache.mOrderPosition >= tabIndex) {
                    tabPositionCache.mTabId = INVALID_TAB_ID;
                }
            } else {
                mOrder.add(tab);
            }
        }

        // Suppress the ADDED event to prevent animation of tabs created via session restore.
        if (mInitialTabsAdded) {
            notifyListeners(tab, TabEvents.ADDED,
                    Integer.toString(getPrivacySpecificTabIndex(tabIndex, isPrivate)));
        }

        return tab;
    }

    /**
     * Return the index, among those tabs whose privacy setting matches {@code isPrivate},
     * of the tab at position {@code index} in {@code mOrder}.
     *
     * @return {@code NEW_LAST_INDEX} when {@code index} is {@code NEW_LAST_INDEX} or no matches were
     * found.
     */
    private int getPrivacySpecificTabIndex(int index, boolean isPrivate) {
        if (index == NEW_LAST_INDEX) {
            return NEW_LAST_INDEX;
        }

        int privacySpecificIndex = -1;
        for (int i = 0; i <= index; i++) {
            final Tab tab = mOrder.get(i);
            if (tab.isPrivate() == isPrivate) {
                privacySpecificIndex++;
            }
        }
        return privacySpecificIndex > -1 ? privacySpecificIndex : NEW_LAST_INDEX;
    }

    public synchronized void removeTab(int id) {
        if (mTabs.containsKey(id)) {
            Tab tab = getTab(id);
            mOrder.remove(tab);
            mTabs.remove(id);
            tabPositionCache.mTabId = INVALID_TAB_ID;
        }
    }

    public synchronized Tab selectTab(int id) {
        if (!mTabs.containsKey(id))
            return null;

        final Tab oldTab = getSelectedTab();
        final Tab tab = mTabs.get(id);

        // This avoids a NPE below, but callers need to be careful to
        // handle this case.
        if (tab == null || oldTab == tab) {
            return tab;
        }

        mSelectedTab = tab;
        mSelectedTab.updatePageAction();

        notifyListeners(tab, TabEvents.SELECTED);

        if (oldTab != null) {
            mPreviouslySelectedTabId = oldTab.getId();
            notifyListeners(oldTab, TabEvents.UNSELECTED);
        }

        // Pass a message to Gecko to update tab state in BrowserApp.
        final GeckoBundle data = new GeckoBundle(2);
        data.putInt("id", tab.getId());
        if (oldTab != null && mTabs.containsKey(oldTab.getId())) {
            data.putInt("previousTabId", oldTab.getId());
        }
        mEventDispatcher.dispatch("Tab:Selected", data);
        EventDispatcher.getInstance().dispatch("Tab:Selected", data);
        return tab;
    }

    public synchronized boolean selectLastTab() {
        if (mOrder.isEmpty()) {
            return false;
        }

        selectTab(mOrder.get(mOrder.size() - 1).getId());
        return true;
    }

    private int getIndexOf(Tab tab) {
        return mOrder.lastIndexOf(tab);
    }

    private Tab getNextTabFrom(Tab tab, boolean getPrivate) {
        int numTabs = mOrder.size();
        int index = getIndexOf(tab);
        for (int i = index + 1; i < numTabs; i++) {
            Tab next = mOrder.get(i);
            if (next.isPrivate() == getPrivate) {
                return next;
            }
        }
        return null;
    }

    private Tab getPreviousTabFrom(Tab tab, boolean getPrivate) {
        int index = getIndexOf(tab);
        for (int i = index - 1; i >= 0; i--) {
            Tab prev = mOrder.get(i);
            if (prev.isPrivate() == getPrivate) {
                return prev;
            }
        }
        return null;
    }

    /**
     * Gets the selected tab.
     *
     * The selected tab can be null immediately after startup, in the worst case until after Gecko
     * is up and running.
     *
     * @return the selected tab, or null if no tabs exist
     */
    @CheckResult
    @Nullable
    public Tab getSelectedTab() {
        return mSelectedTab;
    }

    public boolean isSelectedTab(Tab tab) {
        return tab != null && tab == mSelectedTab;
    }

    public boolean isSelectedTabId(int tabId) {
        final Tab selected = mSelectedTab;
        return selected != null && selected.getId() == tabId;
    }

    @RobocopTarget
    public synchronized Tab getTab(int id) {
        if (id == INVALID_TAB_ID)
            return null;

        if (mTabs.size() == 0)
            return null;

        if (!mTabs.containsKey(id))
           return null;

        return mTabs.get(id);
    }

    public synchronized Tab getTabForApplicationId(final String applicationId) {
        if (applicationId == null) {
            return null;
        }

        for (final Tab tab : mOrder) {
            if (applicationId.equals(tab.getApplicationId())) {
                return tab;
            }
        }

        return null;
    }

    /** Close tab and then select the default next tab */
    @RobocopTarget
    public synchronized void closeTab(Tab tab) {
        closeTab(tab, getNextTab(tab));
    }

    public synchronized void closeTab(Tab tab, Tab nextTab) {
        closeTab(tab, nextTab, false);
    }

    public synchronized void closeTab(Tab tab, boolean showUndoToast) {
        closeTab(tab, getNextTab(tab), showUndoToast);
    }

    /** Close tab and then select nextTab */
    public synchronized void closeTab(final Tab tab, Tab nextTab, boolean showUndoToast) {
        if (tab == null)
            return;

        int tabId = tab.getId();
        removeTab(tabId);

        if (nextTab == null) {
            nextTab = loadUrl(getHomepageForNewTab(mAppContext), LOADURL_NEW_TAB);
        }

        selectTab(nextTab.getId());

        tab.onDestroy();

        // Pass a message to Gecko to update tab state in BrowserApp
        final GeckoBundle data = new GeckoBundle(2);
        data.putInt("tabId", tabId);
        data.putBoolean("showUndoToast", showUndoToast);
        mEventDispatcher.dispatch("Tab:Closed", data);
    }

    /** Return the tab that will be selected by default after this one is closed */
    public Tab getNextTab(Tab tab) {
        Tab selectedTab = getSelectedTab();
        if (selectedTab != tab)
            return selectedTab;

        boolean getPrivate = tab.isPrivate();
        Tab nextTab = getNextTabFrom(tab, getPrivate);
        if (nextTab == null)
            nextTab = getPreviousTabFrom(tab, getPrivate);
        if (nextTab == null && getPrivate) {
            // If there are no private tabs remaining, get the last normal tab.
            Tab lastTab = mOrder.get(mOrder.size() - 1);
            if (!lastTab.isPrivate()) {
                nextTab = lastTab;
            } else {
                nextTab = getPreviousTabFrom(lastTab, false);
            }
        }

        final Tab parentTab = getTab(tab.getParentId());
        if (tab.getParentId() == mPreviouslySelectedTabId && tab.getParentId() != INVALID_TAB_ID && parentTab != null) {
            return parentTab;
        } else {
            return nextTab;
        }
    }

    public Iterable<Tab> getTabsInOrder() {
        return mOrder;
    }

    /**
     * @return the current GeckoApp instance, or throws if
     *         we aren't correctly initialized.
     */
    private synchronized Context getAppContext() {
        if (mAppContext == null) {
            throw new IllegalStateException("Tabs not initialized with a GeckoApp instance.");
        }
        return mAppContext;
    }

    public ContentResolver getContentResolver() {
        return getAppContext().getContentResolver();
    }

    // Make Tabs a singleton class.
    private static class TabsInstanceHolder {
        private static final Tabs INSTANCE = new Tabs();
    }

    @RobocopTarget
    public static Tabs getInstance() {
       return Tabs.TabsInstanceHolder.INSTANCE;
    }

    @Override // BundleEventListener
    public synchronized void handleMessage(final String event, final GeckoBundle message,
                                           final EventCallback callback) {
        if ("Sanitize:ClearHistory".equals(event)) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // Tab session history will be cleared as well,
                    // so we need to reset the navigation buttons.
                    for (final Tab tab : mOrder) {
                        tab.handleButtonStateChange(false, false);
                        notifyListeners(tab, TabEvents.LOCATION_CHANGE, tab.getURL());
                    }
                }
            });
            return;

        } else if ("Tab:GetNextTabId".equals(event)) {
            callback.sendSuccess(getNextTabId());
            return;
        }

        // All other events handled below should contain a tabID property
        final int id = message.getInt("tabID", INVALID_TAB_ID);
        Tab tab = getTab(id);

        // "Tab:Added" is a special case because tab will be null if the tab was just added
        if ("Tab:Added".equals(event)) {
            String url = message.getString("uri");

            if (message.getBoolean("cancelEditMode")) {
                final Tab oldTab = getSelectedTab();
                if (oldTab != null) {
                    oldTab.setIsEditing(false);
                }
            }

            if (message.getBoolean("stub")) {
                if (tab == null) {
                    // Tab was already closed; abort
                    return;
                }
            } else {
                tab = addTab(id, url, message.getBoolean("external"),
                                      message.getInt("parentId"),
                                      message.getString("title"),
                                      message.getBoolean("isPrivate"),
                                      message.getInt("tabIndex"));
                // If we added the tab as a stub, we should have already
                // selected it, so ignore this flag for stubbed tabs.
                if (message.getBoolean("selected"))
                    selectTab(id);
            }

            if (message.getBoolean("delayLoad"))
                tab.setState(Tab.STATE_DELAYED);
            if (message.getBoolean("desktopMode"))
                tab.setDesktopMode(true);
        }

        // Tab was already closed; abort
        if (tab == null) {
            return;
        }

        if ("Tab:Close".equals(event)) {
            closeTab(tab);

        } else if ("Tab:Select".equals(event)) {
            if (message.getBoolean("foreground", false)) {
                GeckoApp.launchOrBringToFront();
            }
            selectTab(tab.getId());

        } else if ("Content:LocationChange".equals(event)) {
            tab.handleLocationChange(message);

        } else if ("Content:SubframeNavigation".equals(event)) {
            tab.handleButtonStateChange(message);
            notifyListeners(tab, TabEvents.LOCATION_CHANGE, tab.getURL());

        } else if ("Content:SecurityChange".equals(event)) {
            tab.updateIdentityData(message.getBundle("identity"));
            tab.updatePageAction();
            notifyListeners(tab, TabEvents.SECURITY_CHANGE);

        } else if ("Content:ContentBlockingEvent".equals(event)) {
            tab.updateTracking(message.getString("tracking"));
            notifyListeners(tab, TabEvents.TRACKING_CHANGE);

        } else if ("Content:StateChange".equals(event)) {
            final int state = message.getInt("state");
            if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) == 0) {
                return;
            }
            if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                      " - page load start");
                final boolean restoring = message.getBoolean("restoring");
                tab.handleDocumentStart(restoring, message.getString("uri"));
                notifyListeners(tab, Tabs.TabEvents.START);
            } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                      " - page load stop");
                tab.handleDocumentStop(message.getBoolean("success"));
                notifyListeners(tab, Tabs.TabEvents.STOP);
            }

        } else if ("Content:LoadError".equals(event)) {
            tab.handleLoadError();
            notifyListeners(tab, Tabs.TabEvents.LOAD_ERROR);

        } else if ("Content:DOMContentLoaded".equals(event)) {
            if (TextUtils.isEmpty(message.getString("errorType"))) {
                tab.handleContentLoaded();
                notifyListeners(tab, TabEvents.LOADED);
            } else {
                tab.handleLoadError();
                notifyListeners(tab, TabEvents.LOAD_ERROR);
            }

        } else if ("Content:PageShow".equals(event)) {
            tab.setLoadedFromCache(message.getBoolean("fromCache"));
            tab.updateUserRequested(message.getString("userRequested"));
            notifyListeners(tab, TabEvents.PAGE_SHOW);

        } else if ("Content:DOMTitleChanged".equals(event)) {
            tab.updateTitle(message.getString("title"));

        } else if ("Link:Favicon".equals(event)) {
            // Add the favicon to the set of available icons for this tab.
            tab.addFavicon(message.getString("href"),
                           message.getInt("size"),
                           message.getString("mime"));

            // Load the favicon. If the tab is still loading, we actually do the load once the
            // page has loaded, in an attempt to prevent the favicon load from having a
            // detrimental effect on page load time.
            if (tab.getState() != Tab.STATE_LOADING) {
                tab.loadFavicon();
            }

        } else if ("Link:Touchicon".equals(event)) {
            tab.addTouchicon(message.getString("href"),
                             message.getInt("size"),
                             message.getString("mime"));

        } else if ("Link:Feed".equals(event)) {
            tab.setHasFeeds(true);
            notifyListeners(tab, TabEvents.LINK_FEED);

        } else if ("Link:OpenSearch".equals(event)) {
            tab.setHasOpenSearch(message.getBoolean("visible"));

        } else if ("Link:Manifest".equals(event)) {
            final String url = message.getString("href");
            final String manifest = message.getString("manifest");

            tab.setManifestUrl(url);
            tab.setWebAppManifest(WebAppManifest.fromString(url, manifest));

        } else if ("DesktopMode:Changed".equals(event)) {
            tab.setDesktopMode(message.getBoolean("desktopMode"));
            notifyListeners(tab, TabEvents.DESKTOP_MODE_CHANGE);

        } else if ("Tab:RecordingChange".equals(event)) {
            tab.setRecording(message.getBoolean("recording"));
            notifyListeners(tab, TabEvents.RECORDING_CHANGE);

        } else if ("Tab:AudioPlayingChange".equals(event)) {
            tab.setIsAudioPlaying(message.getBoolean("isAudioPlaying"));
            notifyListeners(tab, TabEvents.AUDIO_PLAYING_CHANGE);

        } else if ("Tab:MediaPlaybackChange".equals(event)) {
            final String status = message.getString("status");
            if (status.equals("resume")) {
                notifyListeners(tab, TabEvents.MEDIA_PLAYING_RESUME);
            } else {
                tab.setIsMediaPlaying(status.equals("start"));
                notifyListeners(tab, TabEvents.MEDIA_PLAYING_CHANGE);
            }

        } else if ("Tab:SetParentId".equals(event)) {
            tab.setParentId(message.getInt("parentID", INVALID_TAB_ID));
        }
    }

    public void refreshThumbnails() {
        final BrowserDB db = BrowserDB.from(mAppContext);
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                for (final Tab tab : mOrder) {
                    if (tab.getThumbnail() == null) {
                        tab.loadThumbnailFromDB(db);
                    }
                }
            }
        });
    }

    public interface OnTabsChangedListener {
        void onTabChanged(Tab tab, TabEvents msg, String data);
    }

    private static final List<OnTabsChangedListener> TABS_CHANGED_LISTENERS = new CopyOnWriteArrayList<OnTabsChangedListener>();

    public static void registerOnTabsChangedListener(OnTabsChangedListener listener) {
        TABS_CHANGED_LISTENERS.add(listener);
    }

    public static void unregisterOnTabsChangedListener(OnTabsChangedListener listener) {
        TABS_CHANGED_LISTENERS.remove(listener);
    }

    public enum TabEvents {
        CLOSED,
        START,
        LOADED,
        LOAD_ERROR,
        STOP,
        FAVICON,
        THUMBNAIL,
        TITLE,
        SELECTED,
        UNSELECTED,
        ADDED,
        RESTORED,
        MOVED,
        LOCATION_CHANGE,
        MENU_UPDATED,
        PAGE_SHOW,
        LINK_FEED,
        SECURITY_CHANGE,
        TRACKING_CHANGE,
        DESKTOP_MODE_CHANGE,
        RECORDING_CHANGE,
        BOOKMARK_ADDED,
        BOOKMARK_REMOVED,
        AUDIO_PLAYING_CHANGE,
        OPENED_FROM_TABS_TRAY,
        MEDIA_PLAYING_CHANGE,
        MEDIA_PLAYING_RESUME,
        START_EDITING,
    }

    public void notifyListeners(Tab tab, TabEvents msg) {
        notifyListeners(tab, msg, "");
    }

    public void notifyListeners(final Tab tab, final TabEvents msg, final String data) {
        if (tab == null &&
            msg != TabEvents.RESTORED) {
            throw new IllegalArgumentException("onTabChanged:" + msg + " must specify a tab.");
        }

        final Runnable notifier = new Runnable() {
            @Override
            public void run() {
                onTabChanged(tab, msg, data);

                if (TABS_CHANGED_LISTENERS.isEmpty()) {
                    return;
                }

                Iterator<OnTabsChangedListener> items = TABS_CHANGED_LISTENERS.iterator();
                while (items.hasNext()) {
                    items.next().onTabChanged(tab, msg, data);
                }
            }
        };

        if (ThreadUtils.isOnUiThread()) {
            notifier.run();
        } else {
            ThreadUtils.postToUiThread(notifier);
        }
    }

    /* package */ void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch (msg) {
            // We want the tab record to have an accurate favicon, so queue
            // the persisting of tabs when it changes.
            case FAVICON:
            case LOCATION_CHANGE:
                queuePersistAllTabs();
                break;
            case RESTORED:
                mInitialTabsAdded = true;
                break;

            // When one tab is deselected, another one is always selected, so only
            // queue a single persist operation. When tabs are added/closed, they
            // are also selected/unselected, so it would be redundant to also listen
            // for ADDED/CLOSED events.
            case SELECTED:
                if (mGeckoView != null) {
                    final int color = getTabColor(tab);
                    mGeckoView.getSession().getCompositorController().setClearColor(color);
                    mGeckoView.coverUntilFirstPaint(color);
                }
                queuePersistAllTabs();
                tab.onChange();
                break;
            case UNSELECTED:
                tab.onChange();
                break;
            default:
                break;
        }
    }

    /**
     * Queues a request to persist tabs after PERSIST_TABS_AFTER_MILLISECONDS
     * milliseconds have elapsed. If any existing requests are already queued then
     * those requests are removed.
     */
    private void queuePersistAllTabs() {
        final Handler backgroundHandler = ThreadUtils.getBackgroundHandler();

        // Note: Its safe to modify the runnable here because all of the callers are on the same thread.
        if (mPersistTabsRunnable != null) {
            backgroundHandler.removeCallbacks(mPersistTabsRunnable);
            mPersistTabsRunnable = null;
        }

        mPersistTabsRunnable = new PersistTabsRunnable(mAppContext, getTabsInOrder());
        backgroundHandler.postDelayed(mPersistTabsRunnable, PERSIST_TABS_AFTER_MILLISECONDS);
    }

    /**
     * Looks for an open tab with the given URL.
     * @param url       the URL of the tab we're looking for
     *
     * @return first Tab with the given URL, or null if there is no such tab.
     */
    @RobocopTarget
    public Tab getFirstTabForUrl(String url) {
        return getFirstTabForUrlHelper(url, null);
    }

    /**
     * Looks for an open tab with the given URL and private state.
     * @param url       the URL of the tab we're looking for
     * @param isPrivate if true, only look for tabs that are private. if false,
     *                  only look for tabs that are non-private.
     *
     * @return first Tab with the given URL, or null if there is no such tab.
     */
    public Tab getFirstTabForUrl(String url, boolean isPrivate) {
        return getFirstTabForUrlHelper(url, isPrivate);
    }

    private Tab getFirstTabForUrlHelper(String url, Boolean isPrivate) {
        if (url == null) {
            return null;
        }

        for (Tab tab : mOrder) {
            if (isPrivate != null && isPrivate != tab.isPrivate()) {
                continue;
            }
            if (url.equals(tab.getURL())) {
                return tab;
            }
        }

        return null;
    }

    /**
     * Looks for the last open tab with the given URL and private state.
     * @param url       the URL of the tab we're looking for
     *
     * @return last Tab with the given URL, or null if there is no such tab.
     */
    public Tab getLastTabForUrl(String url) {
        if (url == null) {
            return null;
        }

        Tab lastTab = null;
        for (Tab tab : mOrder) {
            if (url.equals(tab.getURL())) {
                lastTab = tab;
            }
        }

        return lastTab;
    }

    /**
     * Looks for a reader mode enabled open tab with the given URL and private
     * state.
     *
     * @param url
     *            The URL of the tab we're looking for. The url parameter can be
     *            the actual article URL or the reader mode article URL.
     * @param isPrivate
     *            If true, only look for tabs that are private. If false, only
     *            look for tabs that are not private.
     *
     * @return The first Tab with the given URL, or null if there is no such
     *         tab.
     */
    public Tab getFirstReaderTabForUrl(String url, boolean isPrivate) {
        if (url == null) {
            return null;
        }

        url = ReaderModeUtils.stripAboutReaderUrl(url);

        for (Tab tab : mOrder) {
            if (isPrivate != tab.isPrivate()) {
                continue;
            }
            String tabUrl = tab.getURL();
            if (AboutPages.isAboutReader(tabUrl)) {
                tabUrl = ReaderModeUtils.stripAboutReaderUrl(tabUrl);
                if (url.equals(tabUrl)) {
                    return tab;
                }
            }
        }

        return null;
    }

    /**
     * Loads a tab with the given URL in the currently selected tab.
     *
     * @param url URL of page to load
     */
    @RobocopTarget
    public Tab loadUrl(String url) {
        return loadUrl(url, LOADURL_NONE);
    }

    /**
     * Loads a tab with the given URL.
     *
     * @param url   URL of page to load
     * @param flags flags used to load tab
     *
     * @return      the Tab if a new one was created; null otherwise
     */
    @RobocopTarget
    public Tab loadUrl(String url, int flags) {
        return loadUrl(url, null, null, INVALID_TAB_ID, null, flags);
    }

    public Tab loadUrlWithIntentExtras(final String url, final SafeIntent intent, final int flags) {
        // We can't directly create a listener to tell when the user taps on the "What's new"
        // notification, so we use this intent handling as a signal that they tapped the notification.
        if (intent.getBooleanExtra(WhatsNewReceiver.EXTRA_WHATSNEW_NOTIFICATION, false)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.NOTIFICATION,
                    WhatsNewReceiver.EXTRA_WHATSNEW_NOTIFICATION);
        }

        // Note: we don't get the URL from the intent so the calling
        // method has the opportunity to change the URL if applicable.
        return loadUrl(url, null, null, INVALID_TAB_ID, intent, flags);
    }

    public Tab loadUrl(final String url, final String searchEngine, final int parentId, final int flags) {
        return loadUrl(url, searchEngine, null, parentId, null, flags);
    }

    /**
     * Loads a tab with the given URL.
     *
     * @param url          URL of page to load, or search term used if searchEngine is given
     * @param searchEngine if given, the search engine with this name is used
     *                     to search for the url string; if null, the URL is loaded directly
     * @param referrerUri  the URI which referred this page load, or null if there is none.
     * @param parentId     ID of this tab's parent, or INVALID_TAB_ID (-1) if it has no parent
     * @param intent       an intent whose extras are used to modify the request
     * @param flags        flags used to load tab
     *
     * @return             the Tab if a new one was created; null otherwise
     */
    public Tab loadUrl(final String url, final String searchEngine, @Nullable final String referrerUri,
            final int parentId, @Nullable final SafeIntent intent, final int flags) {
        final GeckoBundle data = new GeckoBundle();
        Tab tabToSelect = null;
        boolean delayLoad = (flags & LOADURL_DELAY_LOAD) != 0;

        // delayLoad implies background tab
        boolean background = delayLoad || (flags & LOADURL_BACKGROUND) != 0;

        boolean isPrivate = (flags & LOADURL_PRIVATE) != 0 || (intent != null && intent.getBooleanExtra(PRIVATE_TAB_INTENT_EXTRA, false));
        boolean userEntered = (flags & LOADURL_USER_ENTERED) != 0;
        boolean desktopMode = (flags & LOADURL_DESKTOP) != 0;
        boolean external = (flags & LOADURL_EXTERNAL) != 0;
        final boolean isFirstShownAfterActivityUnhidden = (flags & LOADURL_FIRST_AFTER_ACTIVITY_UNHIDDEN) != 0;
        final boolean startEditing = (flags & LOADURL_START_EDITING) != 0;

        data.putString("url", url);
        data.putString("engine", searchEngine);
        data.putInt("parentId", parentId);
        data.putBoolean("userEntered", userEntered);
        data.putBoolean("isPrivate", isPrivate);
        data.putBoolean("pinned", (flags & LOADURL_PINNED) != 0);
        data.putBoolean("desktopMode", desktopMode);
        data.putString("referrerURI", referrerUri);

        final boolean needsNewTab;
        final String applicationId = (intent == null) ? null :
                intent.getStringExtra(Browser.EXTRA_APPLICATION_ID);
        if (applicationId == null) {
            needsNewTab = (flags & LOADURL_NEW_TAB) != 0;
        } else {
            // If you modify this code, be careful that intent != null.
            final boolean extraCreateNewTab = intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false);
            final Tab applicationTab = getTabForApplicationId(applicationId);
            if (applicationTab == null || extraCreateNewTab) {
                needsNewTab = true;
            } else {
                needsNewTab = false;
                delayLoad = false;
                background = false;

                tabToSelect = applicationTab;
                final int tabToSelectId = tabToSelect.getId();
                data.putInt("tabID", tabToSelectId);

                // This must be called before the "Tab:Load" event is sent. I think addTab gets
                // away with it because having "newTab" == true causes the selected tab to be
                // updated in JS for the "Tab:Load" event but "newTab" is false in our case.
                // This makes me think the other selectTab is not necessary (bug 1160673).
                //
                // Note: that makes the later call redundant but selectTab exits early so I'm
                // fine not adding the complex logic to avoid calling it again.
                selectTab(tabToSelect.getId());
            }
        }

        data.putBoolean("newTab", needsNewTab);
        data.putBoolean("delayLoad", delayLoad);
        data.putBoolean("selected", !background);

        if (needsNewTab) {
            int tabId = getNextTabId();
            data.putInt("tabID", tabId);

            // The URL is updated for the tab once Gecko responds with the
            // Tab:Added data. We can preliminarily set the tab's URL as
            // long as it's a valid URI.
            String tabUrl = (url != null && Uri.parse(url).getScheme() != null) ? url : null;

            // Add the new tab to the end of the tab order.
            final int tabIndex = NEW_LAST_INDEX;

            tabToSelect = addTab(tabId, tabUrl, external, parentId, url, isPrivate, tabIndex);
            tabToSelect.setDesktopMode(desktopMode);
            tabToSelect.setApplicationId(applicationId);

            if (isFirstShownAfterActivityUnhidden) {
                // We just opened Firefox so we want to show
                // the toolbar but not animate it to avoid jank.
                tabToSelect.setShouldShowToolbarWithoutAnimationOnFirstSelection(true);
            }
        }

        mEventDispatcher.dispatch("Tab:Load", data);

        if (tabToSelect == null) {
            return null;
        }

        if (!delayLoad && !background) {
            selectTab(tabToSelect.getId());
        }

        if (startEditing) {
            notifyListeners(tabToSelect, TabEvents.START_EDITING);
        }

        // Load favicon instantly for about:home page because it's already cached
        if (AboutPages.isBuiltinIconPage(url)) {
            tabToSelect.loadFavicon();
        }

        return tabToSelect;
    }

    /**
     * Opens a new tab and loads a page according to the user's preferences (by default about:home).
     */
    @RobocopTarget
    public Tab addTab() {
        return addTab(Tabs.LOADURL_NONE);
    }

    /**
     * Opens a new tab and loads a page according to the user's preferences (by default about:home).
     */
    @RobocopTarget
    public Tab addPrivateTab() {
        return addTab(Tabs.LOADURL_PRIVATE);
    }

    /**
     * Opens a new tab and loads a page according to the user's preferences (by default about:home).
     *
     * @param flags additional flags used when opening the tab
     */
    public Tab addTab(int flags) {
        return loadUrl(getHomepageForNewTab(mAppContext), flags | Tabs.LOADURL_NEW_TAB);
    }

    /**
     * Open the url as a new tab, and mark the selected tab as its "parent".
     *
     * If the url is already open in a tab, the existing tab is selected.
     * Use this for tabs opened by the browser chrome, so users can press the
     * "Back" button to return to the previous tab.
     *
     * This method will open a new private tab if the currently selected tab
     * is also private.
     *
     * @param url URL of page to load
     */
    public void loadUrlInTab(String url) {
        Iterable<Tab> tabs = getTabsInOrder();
        for (Tab tab : tabs) {
            if (url.equals(tab.getURL())) {
                selectTab(tab.getId());
                return;
            }
        }

        // getSelectedTab() can return null if no tab has been created yet
        // (i.e., we're restoring a session after a crash). In these cases,
        // don't mark any tabs as a parent.
        int parentId = INVALID_TAB_ID;
        int flags = LOADURL_NEW_TAB;

        final Tab selectedTab = getSelectedTab();
        if (selectedTab != null) {
            parentId = selectedTab.getId();
            if (selectedTab.isPrivate()) {
                flags = flags | LOADURL_PRIVATE;
            }
        }

        loadUrl(url, null, parentId, flags);
    }

    /**
     * Gets the next tab ID.
     */
    private static int getNextTabId() {
        return sTabId.getAndIncrement();
    }

    private int getTabColor(Tab tab) {
        if (tab != null) {
            return tab.isPrivate() ? mPrivateClearColor : Color.WHITE;
        }

        return Color.WHITE;
    }

    /** Holds a tab id and the index in {@code mOrder} of the tab having that id. */
    private static class TabPositionCache {
        int mTabId;
        int mOrderPosition;

        TabPositionCache() {
            mTabId = INVALID_TAB_ID;
        }

        void cache(int tabId, int position) {
            mTabId = tabId;
            mOrderPosition = position;
        }
    }

    /**
     * Search for {@code tabId} in {@code mOrder}, starting from position {@code positionHint}.
     * Callers must be synchronized.
     * @param tabId The tab id of the tab being searched for.
     * @param positionHint Must be less than or equal to the actual position in mOrder if searching
     *                     forward, otherwise must be greater than or equal to the actual position.
     * @param searchForward Search forward from {@code positionHint} if true, otherwise search
     *                      backward from {@code positionHint}.
     * @return The position in mOrder of the tab with tab id {@code tabId}.
     */
    private int getOrderPositionForTab(int tabId, int positionHint, boolean searchForward) {
        final int step = searchForward ? 1 : -1;
        final int stopPosition = searchForward ? mOrder.size() : -1;
        for (int i = positionHint; i != stopPosition; i += step) {
            if (mOrder.get(i).getId() == tabId) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @param fromTabId Id of the tab to move.
     * @param fromPositionHint Position of the from tab amongst either all non-private tabs or all
     *                         private tabs, whichever the caller happens to be moving in.
     * @param toTabId Id of the tab in the position the from tab should be moved to.
     * @param toPositionHint Position of the to tab amongst either all non-private tabs or all
     *                       private tabs, whichever the caller happens to be moving in.
     */
    @UiThread
    public void moveTab(int fromTabId, int fromPositionHint, int toTabId, int toPositionHint) {
        if (fromPositionHint == toPositionHint) {
            return;
        }

        // The provided position hints index into either all private tabs or all non-private tabs,
        // but we need the indices with respect to mOrder, which lists all tabs, both private and
        // non-private, in the order in which they were added.

        // The positions in mOrder of the from and to tabs.
        final int fromPosition;
        final int toPosition;
        synchronized (this) {
            if (tabPositionCache.mTabId == fromTabId) {
                fromPosition = tabPositionCache.mOrderPosition;
            } else {
                fromPosition = getOrderPositionForTab(fromTabId, fromPositionHint, true);
            }

            // Start the toPosition search from the mOrder from position.
            final int adjustedToPositionHint = fromPosition + (toPositionHint - fromPositionHint);
            toPosition = getOrderPositionForTab(toTabId, adjustedToPositionHint, fromPositionHint < toPositionHint);
            // Remember where the tab was moved to so that if this move continues we'll be ready.
            tabPositionCache.cache(fromTabId, toPosition);

            if (fromPosition == -1 || toPosition == -1) {
                throw new IllegalStateException("Tabs search failed: (" + fromPositionHint + ", " + toPositionHint + ")" +
                        " --> (" + fromPosition + ", " + toPosition + ")");
            }

            // Updating mOrder requires the creation of two new tabs arrays, one for newTabsArray,
            // and one when mOrder is reassigned - note that that's the best we can ever do (as long
            // as mOrder is a CopyOnWriteArrayList) even if fromPosition and toPosition only differ
            // by one, which is the common case.
            final Tab[] newTabsArray = new Tab[mOrder.size()];
            mOrder.toArray(newTabsArray);
            // Creates a List backed by newTabsArray.
            final List<Tab> newTabsList = Arrays.asList(newTabsArray);
            JavaUtil.moveInList(newTabsList, fromPosition, toPosition);
            // (Note that there's no way to atomically update the current mOrder with our new list,
            // and hence no way (short of synchronizing all readers of mOrder) to prevent readers on
            // other threads from possibly choosing to start iterating mOrder in between when we
            // might mOrder.clear() and then mOrder.addAll(newTabsArray) if we were to repopulate
            // mOrder in place.)
            mOrder = new CopyOnWriteArrayList<>(newTabsList);
        }

        queuePersistAllTabs();

        notifyListeners(mOrder.get(toPosition), TabEvents.MOVED);

        final GeckoBundle data = new GeckoBundle();
        data.putInt("fromTabId", fromTabId);
        data.putInt("fromPosition", fromPosition);
        data.putInt("toTabId", toTabId);
        data.putInt("toPosition", toPosition);
        mEventDispatcher.dispatch("Tab:Move", data);
    }

    /**
     * @return True if the homepage preference is not empty.
     */
    public static boolean hasHomepage(Context context) {
        return !TextUtils.isEmpty(getHomepage(context));
    }

    /**
     * Note: For opening a new tab while respecting the user's preferences, just use
     *       {@link Tabs#addTab()} instead.
     *
     * @return The user's homepage (falling back to about:home) if PREFS_HOMEPAGE_FOR_EVERY_NEW_TAB
     *         is enabled, or else about:home.
     */
    @NonNull
    private static String getHomepageForNewTab(Context context) {
        final SharedPreferences preferences = GeckoSharedPrefs.forApp(context);
        final boolean forEveryNewTab = preferences.getBoolean(GeckoPreferences.PREFS_HOMEPAGE_FOR_EVERY_NEW_TAB, false);

        return forEveryNewTab ? getHomepageForStartupTab(context) : AboutPages.HOME;
    }

    /**
     * @return The user's homepage, or about:home if none is set.
     */
    @NonNull
    public static String getHomepageForStartupTab(Context context) {
        final String homepage = Tabs.getHomepage(context);
        return TextUtils.isEmpty(homepage) ? AboutPages.HOME : homepage;
    }

    @Nullable
    public static String getHomepage(Context context) {
        final SharedPreferences preferences = GeckoSharedPrefs.forProfile(context);
        final String homepagePreference = preferences.getString(GeckoPreferences.PREFS_HOMEPAGE, AboutPages.HOME);

        final boolean readFromPartnerProvider = preferences.getBoolean(
                GeckoPreferences.PREFS_READ_PARTNER_CUSTOMIZATIONS_PROVIDER, false);

        if (!readFromPartnerProvider) {
            // Just return homepage as set by the user (or null).
            return homepagePreference;
        }


        final String homepagePrevious = preferences.getString(GeckoPreferences.PREFS_HOMEPAGE_PARTNER_COPY, null);
        if (homepagePrevious != null && !homepagePrevious.equals(homepagePreference)) {
            // We have read the homepage once and the user has changed it since then. Just use the
            // value the user has set.
            return homepagePreference;
        }

        // This is the first time we read the partner provider or the value has not been altered by the user
        final String homepagePartner = PartnerBrowserCustomizationsClient.getHomepage(context);

        if (homepagePartner == null) {
            // We didn't get anything from the provider. Let's just use what we have locally.
            return homepagePreference;
        }

        if (!homepagePartner.equals(homepagePrevious)) {
            // We have a new value. Update the preferences.
            preferences.edit()
                    .putString(GeckoPreferences.PREFS_HOMEPAGE, homepagePartner)
                    .putString(GeckoPreferences.PREFS_HOMEPAGE_PARTNER_COPY, homepagePartner)
                    .apply();
        }

        return homepagePartner;
    }
}
