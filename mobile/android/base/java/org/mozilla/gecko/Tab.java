/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.URLMetadata;
import org.mozilla.gecko.favicons.FaviconGenerator;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.LoadFaviconTask;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.favicons.RemoteFavicon;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.reader.ReadingListHelper;
import org.mozilla.gecko.toolbar.BrowserToolbar.TabEditingState;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import org.mozilla.gecko.widget.SiteLogins;

public class Tab {
    private static final String LOGTAG = "GeckoTab";

    private static Pattern sColorPattern;
    private final int mId;
    private final BrowserDB mDB;
    private long mLastUsed;
    private String mUrl;
    private String mBaseDomain;
    private String mUserRequested; // The original url requested. May be typed by the user or sent by an extneral app for example.
    private String mTitle;
    private Bitmap mFavicon;
    private String mFaviconUrl;
    private String mApplicationId; // Intended to be null after explicit user action.

    // The set of all available Favicons for this tab, sorted by attractiveness.
    final TreeSet<RemoteFavicon> mAvailableFavicons = new TreeSet<>();
    private boolean mHasFeeds;
    private boolean mHasOpenSearch;
    private final SiteIdentity mSiteIdentity;
    private SiteLogins mSiteLogins;
    private BitmapDrawable mThumbnail;
    private final int mParentId;
    // Indicates the url was loaded from a source external to the app. This will be cleared
    // when the user explicitly loads a new url (e.g. clicking a link is not explicit).
    private final boolean mExternal;
    private boolean mBookmark;
    private int mFaviconLoadId;
    private String mContentType;
    private boolean mHasTouchListeners;
    private ZoomConstraints mZoomConstraints;
    private boolean mIsRTL;
    private final ArrayList<View> mPluginViews;
    private final HashMap<Object, Layer> mPluginLayers;
    private int mBackgroundColor;
    private int mState;
    private Bitmap mThumbnailBitmap;
    private boolean mDesktopMode;
    private boolean mEnteringReaderMode;
    private final Context mAppContext;
    private ErrorType mErrorType = ErrorType.NONE;
    private volatile int mLoadProgress;
    private volatile int mRecordingCount;
    private volatile boolean mIsAudioPlaying;
    private String mMostRecentHomePanel;
    private boolean mShouldShowToolbarWithoutAnimationOnFirstSelection;

    /*
     * Bundle containing restore data for the panel referenced in mMostRecentHomePanel. This can be
     * e.g. the most recent folder for the bookmarks panel, or any other state that should be
     * persisted. This is then used e.g. when returning to homepanels via history.
     */
    private Bundle mMostRecentHomePanelData;

    private int mHistoryIndex;
    private int mHistorySize;
    private boolean mCanDoBack;
    private boolean mCanDoForward;

    private boolean mIsEditing;
    private final TabEditingState mEditingState = new TabEditingState();

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
        mDB = GeckoProfile.get(context).getDB();
        mId = id;
        mUrl = url;
        mBaseDomain = "";
        mUserRequested = "";
        mExternal = external;
        mParentId = parentId;
        mTitle = title == null ? "" : title;
        mSiteIdentity = new SiteIdentity();
        mHistoryIndex = -1;
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

    @RobocopTarget
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

    // mUserRequested should never be null, but it may be an empty string
    public synchronized String getUserRequested() {
        return mUserRequested;
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

    /**
     * Returns the base domain of the loaded uri. Note that if the page is
     * a Reader mode uri, the base domain returned is that of the original uri.
     */
    public String getBaseDomain() {
        return mBaseDomain;
    }

    public Bitmap getFavicon() {
        return mFavicon;
    }

    protected String getApplicationId() {
        return mApplicationId;
    }

    protected void setApplicationId(final String applicationId) {
        mApplicationId = applicationId;
    }

    public BitmapDrawable getThumbnail() {
        return mThumbnail;
    }

    public String getMostRecentHomePanel() {
        return mMostRecentHomePanel;
    }

    public Bundle getMostRecentHomePanelData() {
        return mMostRecentHomePanelData;
    }

    public void setMostRecentHomePanel(String panelId) {
        mMostRecentHomePanel = panelId;
        mMostRecentHomePanelData = null;
    }

    public void setMostRecentHomePanelData(Bundle data) {
        mMostRecentHomePanelData = data;
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

    public void updateThumbnail(final Bitmap b, final ThumbnailHelper.CachePolicy cachePolicy) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (b != null) {
                    try {
                        mThumbnail = new BitmapDrawable(mAppContext.getResources(), b);
                        if (mState == Tab.STATE_SUCCESS && cachePolicy == ThumbnailHelper.CachePolicy.STORE) {
                            saveThumbnailToDB(mDB);
                        } else {
                            // If the page failed to load, or requested that we not cache info about it, clear any previous
                            // thumbnails we've stored.
                            clearThumbnailFromDB(mDB);
                        }
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

    public SiteLogins getSiteLogins() {
        return mSiteLogins;
    }

    public boolean isBookmark() {
        return mBookmark;
    }

    public boolean isExternal() {
        return mExternal;
    }

    public synchronized void updateURL(String url) {
        if (url != null && url.length() > 0) {
            mUrl = url;
        }
    }

    public synchronized void updateUserRequested(String userRequested) {
        mUserRequested = userRequested;
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

    public void setMetadata(JSONObject metadata) {
        if (metadata == null) {
            return;
        }

        final ContentResolver cr = mAppContext.getContentResolver();
        final URLMetadata urlMetadata = mDB.getURLMetadata();

        final Map<String, Object> data = urlMetadata.fromJSON(metadata);
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                urlMetadata.save(cr, data);
            }
        });
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

    public int getHistoryIndex() {
        return mHistoryIndex;
    }

    public int getHistorySize() {
        return mHistorySize;
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

    public synchronized void addFavicon(String faviconURL, int faviconSize, String mimeType) {
        RemoteFavicon favicon = new RemoteFavicon(faviconURL, faviconSize, mimeType);

        // Add this Favicon to the set of available Favicons.
        synchronized (mAvailableFavicons) {
            mAvailableFavicons.add(favicon);
        }
    }

    public void loadFavicon() {
        // Static Favicons never change
        if (AboutPages.isBuiltinIconPage(mUrl) && mFavicon != null) {
            return;
        }

        // If we have a Favicon explicitly set, load it.
        if (!mAvailableFavicons.isEmpty()) {
            RemoteFavicon newFavicon = mAvailableFavicons.first();

            // If the new Favicon is different, cancel the old load. Else, abort.
            if (newFavicon.faviconUrl.equals(mFaviconUrl)) {
                return;
            }

            Favicons.cancelFaviconLoad(mFaviconLoadId);
            mFaviconUrl = newFavicon.faviconUrl;
        } else {
            // Otherwise, fallback to the default Favicon.
            mFaviconUrl = null;
        }

        final Favicons.LoadType loadType;
        if (mSiteIdentity.getSecurityMode() == SiteIdentity.SecurityMode.CHROMEUI) {
            loadType = Favicons.LoadType.PRIVILEGED;
        } else {
            loadType = Favicons.LoadType.UNPRIVILEGED;
        }

        int flags = (isPrivate() || mErrorType != ErrorType.NONE) ? 0 : LoadFaviconTask.FLAG_PERSIST;
        mFaviconLoadId = Favicons.getSizedFavicon(mAppContext, mUrl, mFaviconUrl,
                loadType, Favicons.browserToolbarFaviconSize, flags,
                new OnFaviconLoadedListener() {
                    @Override
                    public void onFaviconLoaded(String pageUrl, String faviconURL, Bitmap favicon) {
                        // The tab might be pointing to another URL by the time the
                        // favicon is finally loaded, in which case we simply ignore it.
                        if (!pageUrl.equals(mUrl)) {
                            return;
                        }

                        // That one failed. Try the next one.
                        if (favicon == null) {
                            // If what we just tried to load originated from the set of declared icons..
                            if (!mAvailableFavicons.isEmpty()) {
                                // Discard it.
                                mAvailableFavicons.remove(mAvailableFavicons.first());

                                // Load the next best, if we have one. If not, it'll fall back to the
                                // default Favicon URL, before giving up.
                                loadFavicon();

                                return;
                            }

                            // Total failure: generate a default favicon.
                            FaviconGenerator.generate(mAppContext, mUrl, this);
                            return;
                        }

                        mFavicon = favicon;
                        mFaviconLoadId = Favicons.NOT_LOADING;
                        Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.FAVICON);
                    }
                }
        );
    }

    public synchronized void clearFavicon() {
        // Cancel any ongoing favicon load (if we never finished downloading the old favicon before
        // we changed page).
        Favicons.cancelFaviconLoad(mFaviconLoadId);

        // Keep the favicon unchanged while entering reader mode
        if (mEnteringReaderMode)
            return;

        mFavicon = null;
        mFaviconUrl = null;
        mAvailableFavicons.clear();
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

    public void setLoginInsecure(boolean isInsecure) {
        mSiteIdentity.setLoginInsecure(isInsecure);
    }

    public void setSiteLogins(SiteLogins siteLogins) {
        mSiteLogins = siteLogins;
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
                final String pageUrl = ReaderModeUtils.stripAboutReaderUrl(url);

                mBookmark = mDB.isBookmark(getContentResolver(), pageUrl);
                Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.MENU_UPDATED);
            }
        });
    }

    public void addBookmark() {
        final String url = getURL();
        if (url == null) {
            return;
        }

        final String pageUrl = ReaderModeUtils.stripAboutReaderUrl(getURL());

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                mDB.addBookmark(getContentResolver(), mTitle, pageUrl);
                Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.BOOKMARK_ADDED);
            }
        });

        if (AboutPages.isAboutReader(url)) {
            ReadingListHelper.cacheReaderItem(pageUrl, mId, mAppContext);
        }
    }

    public void removeBookmark() {
        final String url = getURL();
        if (url == null) {
            return;
        }

        final String pageUrl = ReaderModeUtils.stripAboutReaderUrl(getURL());

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                mDB.removeBookmarksWithURL(getContentResolver(), pageUrl);
                Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.BOOKMARK_REMOVED);
            }
        });

        // We need to ensure we remove readercached items here - we could have switched out of readermode
        // before unbookmarking, so we don't necessarily have an about:reader URL here.
        ReadingListHelper.removeCachedReaderItem(pageUrl, mAppContext);
    }

    public boolean isEnteringReaderMode() {
        return mEnteringReaderMode;
    }

    public void doReload(boolean bypassCache) {
        GeckoAppShell.notifyObservers("Session:Reload", "{\"bypassCache\":" + String.valueOf(bypassCache) + "}");
    }

    // Our version of nsSHistory::GetCanGoBack
    public boolean canDoBack() {
        return mCanDoBack;
    }

    public boolean doBack() {
        if (!canDoBack())
            return false;

        GeckoAppShell.notifyObservers("Session:Back", "");
        return true;
    }

    public void doStop() {
        GeckoAppShell.notifyObservers("Session:Stop", "");
    }

    // Our version of nsSHistory::GetCanGoForward
    public boolean canDoForward() {
        return mCanDoForward;
    }

    public boolean doForward() {
        if (!canDoForward())
            return false;

        GeckoAppShell.notifyObservers("Session:Forward", "");
        return true;
    }

    void handleLocationChange(JSONObject message) throws JSONException {
        final String uri = message.getString("uri");
        final String oldUrl = getURL();
        final boolean sameDocument = message.getBoolean("sameDocument");
        mEnteringReaderMode = ReaderModeUtils.isEnteringReaderMode(oldUrl, uri);
        mHistoryIndex = message.getInt("historyIndex");
        mHistorySize = message.getInt("historySize");
        mCanDoBack = message.getBoolean("canGoBack");
        mCanDoForward = message.getBoolean("canGoForward");

        if (!TextUtils.equals(oldUrl, uri)) {
            updateURL(uri);
            updateBookmark();
            if (!sameDocument) {
                // We can unconditionally clear the favicon and title here: we
                // already filtered both cases in which this was a (pseudo-)
                // spurious location change, so we're definitely loading a new
                // page.
                clearFavicon();

                // Load local static Favicons immediately
                if (AboutPages.isBuiltinIconPage(uri)) {
                    loadFavicon();
                }

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
        updateUserRequested(message.getString("userRequested"));
        mBaseDomain = message.optString("baseDomain");

        setHasFeeds(false);
        setHasOpenSearch(false);
        mSiteIdentity.reset();
        setSiteLogins(null);
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

    void handleDocumentStart(boolean restoring, String url) {
        setLoadProgress(LOAD_PROGRESS_START);
        setState((!restoring && shouldShowProgress(url)) ? STATE_LOADING : STATE_SUCCESS);
        mSiteIdentity.reset();
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

    protected void saveThumbnailToDB(final BrowserDB db) {
        final BitmapDrawable thumbnail = mThumbnail;
        if (thumbnail == null) {
            return;
        }

        try {
            final String url = getURL();
            if (url == null) {
                return;
            }

            db.updateThumbnailForUrl(getContentResolver(), url, thumbnail);
        } catch (Exception e) {
            // ignore
        }
    }

    public void loadThumbnailFromDB(final BrowserDB db) {
        try {
            final String url = getURL();
            if (url == null) {
                return;
            }

            byte[] thumbnail = db.getThumbnailForUrl(getContentResolver(), url);
            if (thumbnail == null) {
                return;
            }

            Bitmap bitmap = BitmapUtils.decodeByteArray(thumbnail);
            mThumbnail = new BitmapDrawable(mAppContext.getResources(), bitmap);

            Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.THUMBNAIL);
        } catch (Exception e) {
            // ignore
        }
    }

    private void clearThumbnailFromDB(final BrowserDB db) {
        try {
            final String url = getURL();
            if (url == null) {
                return;
            }

            // Passing in a null thumbnail will delete the stored thumbnail for this url
            db.updateThumbnailForUrl(getContentResolver(), url, null);
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
        synchronized (mPluginLayers) {
            mPluginLayers.put(surfaceOrView, layer);
        }
    }

    public Layer getPluginLayer(Object surfaceOrView) {
        synchronized (mPluginLayers) {
            return mPluginLayers.get(surfaceOrView);
        }
    }

    public Collection<Layer> getPluginLayers() {
        synchronized (mPluginLayers) {
            return new ArrayList<Layer>(mPluginLayers.values());
        }
    }

    public Layer removePluginLayer(Object surfaceOrView) {
        synchronized (mPluginLayers) {
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

    public void setRecording(boolean isRecording) {
        if (isRecording) {
            mRecordingCount++;
        } else {
            mRecordingCount--;
        }
    }

    public boolean isRecording() {
        return mRecordingCount > 0;
    }

    public void setIsAudioPlaying(boolean isAudioPlaying) {
        mIsAudioPlaying = isAudioPlaying;
    }

    public boolean isAudioPlaying() {
        return mIsAudioPlaying;
    }

    public boolean isEditing() {
        return mIsEditing;
    }

    public void setIsEditing(final boolean isEditing) {
        this.mIsEditing = isEditing;
    }

    public TabEditingState getEditingState() {
        return mEditingState;
    }

    public void setShouldShowToolbarWithoutAnimationOnFirstSelection(final boolean shouldShowWithoutAnimation) {
        mShouldShowToolbarWithoutAnimationOnFirstSelection = shouldShowWithoutAnimation;
    }

    public boolean getShouldShowToolbarWithoutAnimationOnFirstSelection() {
        return mShouldShowToolbarWithoutAnimationOnFirstSelection;
    }
}
