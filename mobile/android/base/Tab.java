/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;

public class Tab {
    private static final String LOGTAG = "GeckoTab";

    private static Pattern sColorPattern;
    private final int mId;
    private long mLastUsed;
    private String mUrl;
    private String mBaseDomain;
    private String mUserSearch;
    private String mTitle;
    private Bitmap mFavicon;
    private String mFaviconUrl;
    private int mFaviconSize;
    private boolean mHasFeeds;
    private boolean mHasOpenSearch;
    private SiteIdentity mSiteIdentity;
    private boolean mReaderEnabled;
    private BitmapDrawable mThumbnail;
    private int mHistoryIndex;
    private int mHistorySize;
    private int mParentId;
    private boolean mExternal;
    private boolean mBookmark;
    private boolean mReadingListItem;
    private int mFaviconLoadId;
    private String mContentType;
    private boolean mHasTouchListeners;
    private ZoomConstraints mZoomConstraints;
    private boolean mIsRTL;
    private ArrayList<View> mPluginViews;
    private HashMap<Object, Layer> mPluginLayers;
    private int mBackgroundColor;
    private int mState;
    private Bitmap mThumbnailBitmap;
    private boolean mDesktopMode;
    private boolean mEnteringReaderMode;
    private Context mAppContext;
    private ErrorType mErrorType = ErrorType.NONE;
    private static final int MAX_HISTORY_LIST_SIZE = 50;
    private volatile int mLoadProgress;

    public static final int STATE_DELAYED = 0;
    public static final int STATE_LOADING = 1;
    public static final int STATE_SUCCESS = 2;
    public static final int STATE_ERROR = 3;

    public static final int LOAD_PROGRESS_INIT = 10;
    public static final int LOAD_PROGRESS_START = 20;
    public static final int LOAD_PROGRESS_LOCATION_CHANGE = 60;
    public static final int LOAD_PROGRESS_LOADED = 80;
    public static final int LOAD_PROGRESS_STOP = 100;

    private static final int DEFAULT_BACKGROUND_COLOR = Color.WHITE;

    public enum ErrorType {
        CERT_ERROR,  // Pages with certificate problems
        BLOCKED,     // Pages blocked for phishing or malware warnings
        NET_ERROR,   // All other types of error
        NONE         // Non error pages
    }

    public Tab(Context context, int id, String url, boolean external, int parentId, String title) {
        mAppContext = context.getApplicationContext();
        mId = id;
        mLastUsed = 0;
        mUrl = url;
        mBaseDomain = "";
        mUserSearch = "";
        mExternal = external;
        mParentId = parentId;
        mTitle = title == null ? "" : title;
        mFavicon = null;
        mFaviconUrl = null;
        mFaviconSize = 0;
        mHasFeeds = false;
        mHasOpenSearch = false;
        mSiteIdentity = new SiteIdentity();
        mReaderEnabled = false;
        mEnteringReaderMode = false;
        mThumbnail = null;
        mHistoryIndex = -1;
        mHistorySize = 0;
        mBookmark = false;
        mReadingListItem = false;
        mFaviconLoadId = 0;
        mContentType = "";
        mZoomConstraints = new ZoomConstraints(false);
        mPluginViews = new ArrayList<View>();
        mPluginLayers = new HashMap<Object, Layer>();
        mState = shouldShowProgress(url) ? STATE_LOADING : STATE_SUCCESS;
        mLoadProgress = LOAD_PROGRESS_INIT;

        // At startup, the background is set to a color specified by LayerView
        // when the LayerView is created. Shortly after, this background color
        // will be used before the tab's content is shown.
        mBackgroundColor = DEFAULT_BACKGROUND_COLOR;

        updateBookmark();
    }

    private ContentResolver getContentResolver() {
        return mAppContext.getContentResolver();
    }

    public void onDestroy() {
        Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.CLOSED);
    }

    public int getId() {
        return mId;
    }

    public synchronized void onChange() {
        mLastUsed = System.currentTimeMillis();
    }

    public synchronized long getLastUsed() {
        return mLastUsed;
    }

    public int getParentId() {
        return mParentId;
    }

    // may be null if user-entered query hasn't yet been resolved to a URI
    public synchronized String getURL() {
        return mUrl;
    }

    // mUserSearch should never be null, but it may be an empty string
    public synchronized String getUserSearch() {
        return mUserSearch;
    }

    // mTitle should never be null, but it may be an empty string
    public synchronized String getTitle() {
        return mTitle;
    }

    public String getDisplayTitle() {
        if (mTitle != null && mTitle.length() > 0) {
            return mTitle;
        }

        return mUrl;
    }

    public String getBaseDomain() {
        return mBaseDomain;
    }

    public Bitmap getFavicon() {
        return mFavicon;
    }

    public BitmapDrawable getThumbnail() {
        return mThumbnail;
    }

    public Bitmap getThumbnailBitmap(int width, int height) {
        if (mThumbnailBitmap != null) {
            // Bug 787318 - Honeycomb has a bug with bitmap caching, we can't
            // reuse the bitmap there.
            boolean honeycomb = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB
                              && Build.VERSION.SDK_INT <= Build.VERSION_CODES.HONEYCOMB_MR2);
            boolean sizeChange = mThumbnailBitmap.getWidth() != width
                              || mThumbnailBitmap.getHeight() != height;
            if (honeycomb || sizeChange) {
                mThumbnailBitmap = null;
            }
        }

        if (mThumbnailBitmap == null) {
            Bitmap.Config config = (GeckoAppShell.getScreenDepth() == 24) ?
                Bitmap.Config.ARGB_8888 : Bitmap.Config.RGB_565;
            mThumbnailBitmap = Bitmap.createBitmap(width, height, config);
        }

        return mThumbnailBitmap;
    }

    public void updateThumbnail(final Bitmap b) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (b != null) {
                    try {
                        mThumbnail = new BitmapDrawable(mAppContext.getResources(), b);
                        if (mState == Tab.STATE_SUCCESS)
                            saveThumbnailToDB();
                    } catch (OutOfMemoryError oom) {
                        Log.w(LOGTAG, "Unable to create/scale bitmap.", oom);
                        mThumbnail = null;
                    }
                } else {
                    mThumbnail = null;
                }

                Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.THUMBNAIL);
            }
        });
    }

    public synchronized String getFaviconURL() {
        return mFaviconUrl;
    }

    public boolean hasFeeds() {
        return mHasFeeds;
    }

    public boolean hasOpenSearch() {
        return mHasOpenSearch;
    }

    public SiteIdentity getSiteIdentity() {
        return mSiteIdentity;
    }

    public boolean getReaderEnabled() {
        return mReaderEnabled;
    }

    public boolean isBookmark() {
        return mBookmark;
    }

    public boolean isReadingListItem() {
        return mReadingListItem;
    }

    public boolean isExternal() {
        return mExternal;
    }

    public synchronized void updateURL(String url) {
        if (url != null && url.length() > 0) {
            mUrl = url;
        }
    }

    private synchronized void updateUserSearch(String userSearch) {
        mUserSearch = userSearch;
    }

    public void setErrorType(String type) {
        if ("blocked".equals(type))
            setErrorType(ErrorType.BLOCKED);
        else if ("certerror".equals(type))
            setErrorType(ErrorType.CERT_ERROR);
        else if ("neterror".equals(type))
            setErrorType(ErrorType.NET_ERROR);
        else
            setErrorType(ErrorType.NONE);
    }

    public void setErrorType(ErrorType type) {
        mErrorType = type;
    }

    public ErrorType getErrorType() {
        return mErrorType;
    }

    public void setContentType(String contentType) {
        mContentType = (contentType == null) ? "" : contentType;
    }

    public String getContentType() {
        return mContentType;
    }

    public synchronized void updateTitle(String title) {
        // Keep the title unchanged while entering reader mode.
        if (mEnteringReaderMode) {
            return;
        }

        // If there was a title, but it hasn't changed, do nothing.
        if (mTitle != null &&
            TextUtils.equals(mTitle, title)) {
            return;
        }

        mTitle = (title == null ? "" : title);
        Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.TITLE);
    }

    public void setState(int state) {
        mState = state;

        if (mState != Tab.STATE_LOADING)
            mEnteringReaderMode = false;
    }

    public int getState() {
        return mState;
    }

    public void setZoomConstraints(ZoomConstraints constraints) {
        mZoomConstraints = constraints;
    }

    public ZoomConstraints getZoomConstraints() {
        return mZoomConstraints;
    }

    public void setIsRTL(boolean aIsRTL) {
        mIsRTL = aIsRTL;
    }

    public boolean getIsRTL() {
        return mIsRTL;
    }

    public void setHasTouchListeners(boolean aValue) {
        mHasTouchListeners = aValue;
    }

    public boolean getHasTouchListeners() {
        return mHasTouchListeners;
    }

    public void setFaviconLoadId(int faviconLoadId) {
        mFaviconLoadId = faviconLoadId;
    }

    public int getFaviconLoadId() {
        return mFaviconLoadId;
    }

    /**
     * Returns true if the favicon changed.
     */
    public boolean updateFavicon(Bitmap favicon) {
        if (mFavicon == favicon) {
            return false;
        }
        mFavicon = favicon;
        return true;
    }

    public synchronized void updateFaviconURL(String faviconUrl, int size) {
        // If we already have an "any" sized icon, don't update the icon.
        if (mFaviconSize == -1)
            return;

        // Only update the favicon if it's bigger than the current favicon.
        // We use -1 to represent icons with sizes="any".
        if (size == -1 || size >= mFaviconSize) {
            mFaviconUrl = faviconUrl;
            mFaviconSize = size;
        }
    }

    public synchronized void clearFavicon() {
        // Keep the favicon unchanged while entering reader mode
        if (mEnteringReaderMode)
            return;

        mFavicon = null;
        mFaviconUrl = null;
        mFaviconSize = 0;
    }

    public void setHasFeeds(boolean hasFeeds) {
        mHasFeeds = hasFeeds;
    }

    public void setHasOpenSearch(boolean hasOpenSearch) {
        mHasOpenSearch = hasOpenSearch;
    }

    public void updateIdentityData(JSONObject identityData) {
        mSiteIdentity.update(identityData);
    }

    public void setReaderEnabled(boolean readerEnabled) {
        mReaderEnabled = readerEnabled;
        Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.MENU_UPDATED);
    }

    void updateBookmark() {
        if (getURL() == null) {
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final String url = getURL();
                if (url == null) {
                    return;
                }

                final int flags = BrowserDB.getItemFlags(getContentResolver(), url);
                mBookmark = (flags & Bookmarks.FLAG_BOOKMARK) > 0;
                mReadingListItem = (flags & Bookmarks.FLAG_READING) > 0;
                Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.MENU_UPDATED);
            }
        });
    }

    public void addBookmark() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                String url = getURL();
                if (url == null)
                    return;

                BrowserDB.addBookmark(getContentResolver(), mTitle, url);
            }
        });
    }

    public void removeBookmark() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                String url = getURL();
                if (url == null)
                    return;

                BrowserDB.removeBookmarksWithURL(getContentResolver(), url);
            }
        });
    }

    public void toggleReaderMode() {
        if (AboutPages.isAboutReader(mUrl)) {
            Tabs.getInstance().loadUrl(ReaderModeUtils.getUrlFromAboutReader(mUrl));
        } else if (mReaderEnabled) {
            mEnteringReaderMode = true;
            Tabs.getInstance().loadUrl(ReaderModeUtils.getAboutReaderForUrl(mUrl, mId));
        }
    }

    public boolean isEnteringReaderMode() {
        return mEnteringReaderMode;
    }

    public void doReload() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Reload", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    // Our version of nsSHistory::GetCanGoBack
    public boolean canDoBack() {
        return mHistoryIndex > 0;
    }

    public boolean doBack() {
        if (!canDoBack())
            return false;

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Back", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public boolean showBackHistory() {
        if (!canDoBack())
            return false;
        return this.showHistory(Math.max(mHistoryIndex - MAX_HISTORY_LIST_SIZE, 0), mHistoryIndex, mHistoryIndex);
    }

    public boolean showForwardHistory() {
        if (!canDoForward())
            return false;
        return this.showHistory(mHistoryIndex, Math.min(mHistorySize - 1, mHistoryIndex + MAX_HISTORY_LIST_SIZE), mHistoryIndex);
    }

    public boolean showAllHistory() {
        if (!canDoForward() && !canDoBack())
            return false;

        int min = mHistoryIndex - MAX_HISTORY_LIST_SIZE / 2;
        int max = mHistoryIndex + MAX_HISTORY_LIST_SIZE / 2;
        if (min < 0) {
            max -= min;
        }
        if (max > mHistorySize - 1) {
            min -= max - (mHistorySize - 1);
            max = mHistorySize - 1;
        }
        min = Math.max(min, 0);

        return this.showHistory(min, max, mHistoryIndex);
    }

    /**
     * This method will show the history starting on fromIndex until toIndex of the history.
     */
    public boolean showHistory(int fromIndex, int toIndex, int selIndex) {
        JSONObject json = new JSONObject();
        try {
            json.put("fromIndex", fromIndex);
            json.put("toIndex", toIndex);
            json.put("selIndex", selIndex);
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error", e);
        }
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:ShowHistory", json.toString());
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    public void doStop() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Stop", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    // Our version of nsSHistory::GetCanGoForward
    public boolean canDoForward() {
        return mHistoryIndex < mHistorySize - 1;
    }

    public boolean doForward() {
        if (!canDoForward())
            return false;

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Session:Forward", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    void handleSessionHistoryMessage(String event, JSONObject message) throws JSONException {
        if (event.equals("New")) {
            final String url = message.getString("url");
            mHistoryIndex++;
            mHistorySize = mHistoryIndex + 1;
        } else if (event.equals("Back")) {
            if (!canDoBack()) {
                Log.w(LOGTAG, "Received unexpected back notification");
                return;
            }
            mHistoryIndex--;
        } else if (event.equals("Forward")) {
            if (!canDoForward()) {
                Log.w(LOGTAG, "Received unexpected forward notification");
                return;
            }
            mHistoryIndex++;
        } else if (event.equals("Goto")) {
            int index = message.getInt("index");
            if (index < 0 || index >= mHistorySize) {
                Log.w(LOGTAG, "Received unexpected history-goto notification");
                return;
            }
            mHistoryIndex = index;
        } else if (event.equals("Purge")) {
            int numEntries = message.getInt("numEntries");
            if (numEntries > mHistorySize) {
                Log.w(LOGTAG, "Received unexpectedly large number of history entries to purge");
                mHistoryIndex = -1;
                mHistorySize = 0;
                return;
            }

            mHistorySize -= numEntries;
            mHistoryIndex -= numEntries;

            // If we weren't at the last history entry, mHistoryIndex may have become too small
            if (mHistoryIndex < -1)
                mHistoryIndex = -1;
        }
    }

    void handleLocationChange(JSONObject message) throws JSONException {
        final String uri = message.getString("uri");
        final String oldUrl = getURL();
        final boolean sameDocument = message.getBoolean("sameDocument");
        mEnteringReaderMode = ReaderModeUtils.isEnteringReaderMode(oldUrl, uri);

        if (!TextUtils.equals(oldUrl, uri)) {
            updateURL(uri);
            updateBookmark();
            if (!sameDocument) {
                // We can unconditionally clear the favicon and title here: we
                // already filtered both cases in which this was a (pseudo-)
                // spurious location change, so we're definitely loading a new
                // page.
                clearFavicon();
                updateTitle(null);
            }
        }

        if (sameDocument) {
            // We can get a location change event for the same document with an anchor tag
            // Notify listeners so that buttons like back or forward will update themselves
            Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.LOCATION_CHANGE, oldUrl);
            return;
        }

        setContentType(message.getString("contentType"));
        updateUserSearch(message.getString("userSearch"));
        mBaseDomain = message.optString("baseDomain");

        setHasFeeds(false);
        setHasOpenSearch(false);
        updateIdentityData(null);
        setReaderEnabled(false);
        setZoomConstraints(new ZoomConstraints(true));
        setHasTouchListeners(false);
        setBackgroundColor(DEFAULT_BACKGROUND_COLOR);
        setErrorType(ErrorType.NONE);
        setLoadProgressIfLoading(LOAD_PROGRESS_LOCATION_CHANGE);

        Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.LOCATION_CHANGE, oldUrl);
    }

    private static boolean shouldShowProgress(final String url) {
        return !AboutPages.isAboutPage(url);
    }

    void handleDocumentStart(boolean showProgress, String url) {
        setLoadProgress(LOAD_PROGRESS_START);
        setState(showProgress ? STATE_LOADING : STATE_SUCCESS);
        updateIdentityData(null);
        setReaderEnabled(false);
    }

    void handleDocumentStop(boolean success) {
        setState(success ? STATE_SUCCESS : STATE_ERROR);

        final String oldURL = getURL();
        final Tab tab = this;
        tab.setLoadProgress(LOAD_PROGRESS_STOP);
        ThreadUtils.getBackgroundHandler().postDelayed(new Runnable() {
            @Override
            public void run() {
                // tab.getURL() may return null
                if (!TextUtils.equals(oldURL, getURL()))
                    return;

                ThumbnailHelper.getInstance().getAndProcessThumbnailFor(tab);
            }
        }, 500);
    }

    void handleContentLoaded() {
        setLoadProgressIfLoading(LOAD_PROGRESS_LOADED);
    }

    protected void saveThumbnailToDB() {
        try {
            String url = getURL();
            if (url == null)
                return;

            BrowserDB.updateThumbnailForUrl(getContentResolver(), url, mThumbnail);
        } catch (Exception e) {
            // ignore
        }
    }

    public void addPluginView(View view) {
        mPluginViews.add(view);
    }

    public void removePluginView(View view) {
        mPluginViews.remove(view);
    }

    public View[] getPluginViews() {
        return mPluginViews.toArray(new View[mPluginViews.size()]);
    }

    public void addPluginLayer(Object surfaceOrView, Layer layer) {
        synchronized(mPluginLayers) {
            mPluginLayers.put(surfaceOrView, layer);
        }
    }

    public Layer getPluginLayer(Object surfaceOrView) {
        synchronized(mPluginLayers) {
            return mPluginLayers.get(surfaceOrView);
        }
    }

    public Collection<Layer> getPluginLayers() {
        synchronized(mPluginLayers) {
            return new ArrayList<Layer>(mPluginLayers.values());
        }
    }

    public Layer removePluginLayer(Object surfaceOrView) {
        synchronized(mPluginLayers) {
            return mPluginLayers.remove(surfaceOrView);
        }
    }

    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    /** Sets a new color for the background. */
    public void setBackgroundColor(int color) {
        mBackgroundColor = color;
    }

    /** Parses and sets a new color for the background. */
    public void setBackgroundColor(String newColor) {
        setBackgroundColor(parseColorFromGecko(newColor));
    }

    // Parses a color from an RGB triple of the form "rgb([0-9]+, [0-9]+, [0-9]+)". If the color
    // cannot be parsed, returns white.
    private static int parseColorFromGecko(String string) {
        if (sColorPattern == null) {
            sColorPattern = Pattern.compile("rgb\\((\\d+),\\s*(\\d+),\\s*(\\d+)\\)");
        }

        Matcher matcher = sColorPattern.matcher(string);
        if (!matcher.matches()) {
            return Color.WHITE;
        }

        int r = Integer.parseInt(matcher.group(1));
        int g = Integer.parseInt(matcher.group(2));
        int b = Integer.parseInt(matcher.group(3));
        return Color.rgb(r, g, b);
    }

    public void setDesktopMode(boolean enabled) {
        mDesktopMode = enabled;
    }

    public boolean getDesktopMode() {
        return mDesktopMode;
    }

    public boolean isPrivate() {
        return false;
    }

    /**
     * Sets the tab load progress to the given percentage.
     *
     * @param progressPercentage Percentage to set progress to (0-100)
     */
    void setLoadProgress(int progressPercentage) {
        mLoadProgress = progressPercentage;
    }

    /**
     * Sets the tab load progress to the given percentage only if the tab is
     * currently loading.
     *
     * about:neterror can trigger a STOP before other page load events (bug
     * 976426), so any post-START events should make sure the page is loading
     * before updating progress.
     *
     * @param progressPercentage Percentage to set progress to (0-100)
     */
    void setLoadProgressIfLoading(int progressPercentage) {
        if (getState() == STATE_LOADING) {
            setLoadProgress(progressPercentage);
        }
    }

    /**
     * Gets the tab load progress percentage.
     *
     * @return Current progress percentage
     */
    public int getLoadProgress() {
        return mLoadProgress;
    }
}
