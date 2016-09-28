/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

import android.support.annotation.Nullable;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.notifications.WhatsNewReceiver;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

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
import android.provider.Browser;
import android.support.v4.content.ContextCompat;
import android.util.Log;

public class Tabs implements GeckoEventListener {
    private static final String LOGTAG = "GeckoTabs";

    // mOrder and mTabs are always of the same cardinality, and contain the same values.
    private final CopyOnWriteArrayList<Tab> mOrder = new CopyOnWriteArrayList<Tab>();

    // All writes to mSelectedTab must be synchronized on the Tabs instance.
    // In general, it's preferred to always use selectTab()).
    private volatile Tab mSelectedTab;

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

    private static final long PERSIST_TABS_AFTER_MILLISECONDS = 1000 * 2;

    public static final int INVALID_TAB_ID = -1;

    private static final AtomicInteger sTabId = new AtomicInteger(0);
    private volatile boolean mInitialTabsAdded;

    private Context mAppContext;
    private LayerView mLayerView;
    private ContentObserver mBookmarksContentObserver;
    private PersistTabsRunnable mPersistTabsRunnable;
    private int mPrivateClearColor;

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
            "Tab:Added",
            "Tab:Close",
            "Tab:Select",
            "Content:LocationChange",
            "Content:SecurityChange",
            "Content:StateChange",
            "Content:LoadError",
            "Content:PageShow",
            "DOMTitleChanged",
            "Link:Favicon",
            "Link:Touchicon",
            "Link:Feed",
            "Link:OpenSearch",
            "DesktopMode:Changed",
            "Tab:StreamStart",
            "Tab:StreamStop",
            "Tab:AudioPlayingChange",
            "Tab:MediaPlaybackChange");

        mPrivateClearColor = Color.RED;

    }

    public synchronized void attachToContext(Context context, LayerView layerView) {
        final Context appContext = context.getApplicationContext();
        if (mAppContext == appContext) {
            return;
        }

        if (mAppContext != null) {
            // This should never happen.
            Log.w(LOGTAG, "The application context has changed!");
        }

        mAppContext = appContext;
        mLayerView = layerView;
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

    /**
     * Gets the tab count corresponding to the private state of the selected
     * tab.
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
        return -1;
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
            } else {
                mOrder.add(tab);
            }
        }

        // Suppress the ADDED event to prevent animation of tabs created via session restore.
        if (mInitialTabsAdded) {
            notifyListeners(tab, TabEvents.ADDED);
        }

        return tab;
    }

    public synchronized void removeTab(int id) {
        if (mTabs.containsKey(id)) {
            Tab tab = getTab(id);
            mOrder.remove(tab);
            mTabs.remove(id);
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
            return null;
        }

        mSelectedTab = tab;
        notifyListeners(tab, TabEvents.SELECTED);

        if (mLayerView != null) {
            mLayerView.setClearColor(getTabColor(tab));
        }

        if (oldTab != null) {
            notifyListeners(oldTab, TabEvents.UNSELECTED);
        }

        // Pass a message to Gecko to update tab state in BrowserApp.
        GeckoAppShell.notifyObservers("Tab:Selected", String.valueOf(tab.getId()));
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
     * The selected tab can be null if we're doing a session restore after a
     * crash and Gecko isn't ready yet.
     *
     * @return the selected tab, or null if no tabs exist
     */
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
        if (id == -1)
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
            nextTab = loadUrl(AboutPages.HOME, LOADURL_NEW_TAB);
        }

        selectTab(nextTab.getId());

        tab.onDestroy();

        final JSONObject args = new JSONObject();
        try {
            args.put("tabId", String.valueOf(tabId));
            args.put("showUndoToast", showUndoToast);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error building Tab:Closed arguments: " + e);
        }

        // Pass a message to Gecko to update tab state in BrowserApp
        GeckoAppShell.notifyObservers("Tab:Closed", args.toString());
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
            // If there are no private tabs remaining, get the last normal tab
            Tab lastTab = mOrder.get(mOrder.size() - 1);
            if (!lastTab.isPrivate()) {
                nextTab = lastTab;
            } else {
                nextTab = getPreviousTabFrom(lastTab, false);
            }
        }

        Tab parent = getTab(tab.getParentId());
        if (parent != null) {
            // If the next tab is a sibling, switch to it. Otherwise go back to the parent.
            if (nextTab != null && nextTab.getParentId() == tab.getParentId())
                return nextTab;
            else
                return parent;
        }
        return nextTab;
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

    // GeckoEventListener implementation
    @Override
    public void handleMessage(String event, JSONObject message) {
        Log.d(LOGTAG, "handleMessage: " + event);
        try {
            // All other events handled below should contain a tabID property
            int id = message.getInt("tabID");
            Tab tab = getTab(id);

            // "Tab:Added" is a special case because tab will be null if the tab was just added
            if (event.equals("Tab:Added")) {
                String url = message.isNull("uri") ? null : message.getString("uri");

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
                return;
            }

            // Tab was already closed; abort
            if (tab == null)
                return;

            if (event.equals("Tab:Close")) {
                closeTab(tab);
            } else if (event.equals("Tab:Select")) {
                selectTab(tab.getId());
            } else if (event.equals("Content:LocationChange")) {
                tab.handleLocationChange(message);
            } else if (event.equals("Content:SecurityChange")) {
                tab.updateIdentityData(message.getJSONObject("identity"));
                notifyListeners(tab, TabEvents.SECURITY_CHANGE);
            } else if (event.equals("Content:StateChange")) {
                int state = message.getInt("state");
                if ((state & GeckoAppShell.WPL_STATE_IS_NETWORK) != 0) {
                    if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                        boolean restoring = message.getBoolean("restoring");
                        tab.handleDocumentStart(restoring, message.getString("uri"));
                        notifyListeners(tab, Tabs.TabEvents.START);
                    } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                        tab.handleDocumentStop(message.getBoolean("success"));
                        notifyListeners(tab, Tabs.TabEvents.STOP);
                    }
                }
            } else if (event.equals("Content:LoadError")) {
                tab.handleContentLoaded();
                notifyListeners(tab, Tabs.TabEvents.LOAD_ERROR);
            } else if (event.equals("Content:PageShow")) {
                tab.setLoadedFromCache(message.getBoolean("fromCache"));
                tab.updateUserRequested(message.getString("userRequested"));
                notifyListeners(tab, TabEvents.PAGE_SHOW);
            } else if (event.equals("DOMTitleChanged")) {
                tab.updateTitle(message.getString("title"));
            } else if (event.equals("Link:Favicon")) {
                // Add the favicon to the set of available icons for this tab.

                tab.addFavicon(message.getString("href"), message.getInt("size"), message.getString("mime"));

                // Load the favicon. If the tab is still loading, we actually do the load once the
                // page has loaded, in an attempt to prevent the favicon load from having a
                // detrimental effect on page load time.
                if (tab.getState() != Tab.STATE_LOADING) {
                    tab.loadFavicon();
                }
            } else if (event.equals("Link:Touchicon")) {
                tab.addTouchicon(message.getString("href"), message.getInt("size"), message.getString("mime"));
            } else if (event.equals("Link:Feed")) {
                tab.setHasFeeds(true);
                notifyListeners(tab, TabEvents.LINK_FEED);
            } else if (event.equals("Link:OpenSearch")) {
                boolean visible = message.getBoolean("visible");
                tab.setHasOpenSearch(visible);
            } else if (event.equals("DesktopMode:Changed")) {
                tab.setDesktopMode(message.getBoolean("desktopMode"));
                notifyListeners(tab, TabEvents.DESKTOP_MODE_CHANGE);
            } else if (event.equals("Tab:StreamStart")) {
                tab.setRecording(true);
                notifyListeners(tab, TabEvents.RECORDING_CHANGE);
            } else if (event.equals("Tab:StreamStop")) {
                tab.setRecording(false);
                notifyListeners(tab, TabEvents.RECORDING_CHANGE);
            } else if (event.equals("Tab:AudioPlayingChange")) {
                tab.setIsAudioPlaying(message.getBoolean("isAudioPlaying"));
                notifyListeners(tab, TabEvents.AUDIO_PLAYING_CHANGE);
            } else if (event.equals("Tab:MediaPlaybackChange")) {
                tab.setIsMediaPlaying(message.getBoolean("active"));
                notifyListeners(tab, TabEvents.MEDIA_PLAYING_CHANGE);
            }

        } catch (Exception e) {
            Log.w(LOGTAG, "handleMessage threw for " + event, e);
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
        LOCATION_CHANGE,
        MENU_UPDATED,
        PAGE_SHOW,
        LINK_FEED,
        SECURITY_CHANGE,
        DESKTOP_MODE_CHANGE,
        RECORDING_CHANGE,
        BOOKMARK_ADDED,
        BOOKMARK_REMOVED,
        AUDIO_PLAYING_CHANGE,
        OPENED_FROM_TABS_TRAY,
        MEDIA_PLAYING_CHANGE,
    }

    public void notifyListeners(Tab tab, TabEvents msg) {
        notifyListeners(tab, msg, "");
    }

    public void notifyListeners(final Tab tab, final TabEvents msg, final String data) {
        if (tab == null &&
            msg != TabEvents.RESTORED) {
            throw new IllegalArgumentException("onTabChanged:" + msg + " must specify a tab.");
        }

        ThreadUtils.postToUiThread(new Runnable() {
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
        });
    }

    private void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
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
                if (mLayerView != null) {
                    mLayerView.setSurfaceBackgroundColor(getTabColor(tab));
                    mLayerView.setPaintState(LayerView.PAINT_START);
                }
                queuePersistAllTabs();
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
     * @param url URL of page to load, or search term used if searchEngine is given
     */
    @RobocopTarget
    public Tab loadUrl(String url) {
        return loadUrl(url, LOADURL_NONE);
    }

    /**
     * Loads a tab with the given URL.
     *
     * @param url   URL of page to load, or search term used if searchEngine is given
     * @param flags flags used to load tab
     *
     * @return      the Tab if a new one was created; null otherwise
     */
    @RobocopTarget
    public Tab loadUrl(String url, int flags) {
        return loadUrl(url, null, -1, null, flags);
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
        return loadUrl(url, null, -1, intent, flags);
    }

    public Tab loadUrl(final String url, final String searchEngine, final int parentId, final int flags) {
        return loadUrl(url, searchEngine, parentId, null, flags);
    }

    /**
     * Loads a tab with the given URL.
     *
     * @param url          URL of page to load, or search term used if searchEngine is given
     * @param searchEngine if given, the search engine with this name is used
     *                     to search for the url string; if null, the URL is loaded directly
     * @param parentId     ID of this tab's parent, or -1 if it has no parent
     * @param intent       an intent whose extras are used to modify the request
     * @param flags        flags used to load tab
     *
     * @return             the Tab if a new one was created; null otherwise
     */
    public Tab loadUrl(final String url, final String searchEngine, final int parentId,
                   final SafeIntent intent, final int flags) {
        JSONObject args = new JSONObject();
        Tab tabToSelect = null;
        boolean delayLoad = (flags & LOADURL_DELAY_LOAD) != 0;

        // delayLoad implies background tab
        boolean background = delayLoad || (flags & LOADURL_BACKGROUND) != 0;

        try {
            boolean isPrivate = (flags & LOADURL_PRIVATE) != 0;
            boolean userEntered = (flags & LOADURL_USER_ENTERED) != 0;
            boolean desktopMode = (flags & LOADURL_DESKTOP) != 0;
            boolean external = (flags & LOADURL_EXTERNAL) != 0;
            final boolean isFirstShownAfterActivityUnhidden = (flags & LOADURL_FIRST_AFTER_ACTIVITY_UNHIDDEN) != 0;

            args.put("url", url);
            args.put("engine", searchEngine);
            args.put("parentId", parentId);
            args.put("userEntered", userEntered);
            args.put("isPrivate", isPrivate);
            args.put("pinned", (flags & LOADURL_PINNED) != 0);
            args.put("desktopMode", desktopMode);

            final boolean needsNewTab;
            final String applicationId = (intent == null) ? null :
                    intent.getStringExtra(Browser.EXTRA_APPLICATION_ID);
            if (applicationId == null) {
                needsNewTab = (flags & LOADURL_NEW_TAB) != 0;
            } else {
                // If you modify this code, be careful that intent != null.
                final boolean extraCreateNewTab = (Versions.feature12Plus) ?
                        intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false) : false;
                final Tab applicationTab = getTabForApplicationId(applicationId);
                if (applicationTab == null || extraCreateNewTab) {
                    needsNewTab = true;
                } else {
                    needsNewTab = false;
                    delayLoad = false;
                    background = false;

                    tabToSelect = applicationTab;
                    final int tabToSelectId = tabToSelect.getId();
                    args.put("tabID", tabToSelectId);

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

            args.put("newTab", needsNewTab);
            args.put("delayLoad", delayLoad);
            args.put("selected", !background);

            if (needsNewTab) {
                int tabId = getNextTabId();
                args.put("tabID", tabId);

                // The URL is updated for the tab once Gecko responds with the
                // Tab:Added message. We can preliminarily set the tab's URL as
                // long as it's a valid URI.
                String tabUrl = (url != null && Uri.parse(url).getScheme() != null) ? url : null;

                // Add the new tab to the end of the tab order.
                final int tabIndex = -1;

                tabToSelect = addTab(tabId, tabUrl, external, parentId, url, isPrivate, tabIndex);
                tabToSelect.setDesktopMode(desktopMode);
                tabToSelect.setApplicationId(applicationId);
                if (isFirstShownAfterActivityUnhidden) {
                    // We just opened Firefox so we want to show
                    // the toolbar but not animate it to avoid jank.
                    tabToSelect.setShouldShowToolbarWithoutAnimationOnFirstSelection(true);
                }
            }
        } catch (Exception e) {
            Log.w(LOGTAG, "Error building JSON arguments for loadUrl.", e);
        }

        GeckoAppShell.notifyObservers("Tab:Load", args.toString());

        if (tabToSelect == null) {
            return null;
        }

        if (!delayLoad && !background) {
            selectTab(tabToSelect.getId());
        }

        // Load favicon instantly for about:home page because it's already cached
        if (AboutPages.isBuiltinIconPage(url)) {
            tabToSelect.loadFavicon();
        }

        return tabToSelect;
    }

    public Tab addTab() {
        return loadUrl(AboutPages.HOME, Tabs.LOADURL_NEW_TAB);
    }

    public Tab addPrivateTab() {
        return loadUrl(AboutPages.PRIVATEBROWSING, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
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
        int parentId = -1;
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
    @JNITarget
    public static int getNextTabId() {
        return sTabId.getAndIncrement();
    }

    private int getTabColor(Tab tab) {
        if (tab != null) {
            return tab.isPrivate() ? mPrivateClearColor : Color.WHITE;
        }

        return Color.WHITE;
    }
}
