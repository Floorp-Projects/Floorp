/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Map;
import java.util.concurrent.Future;
import java.util.regex.Pattern;

import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.URLMetadata;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequestBuilder;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.pwa.PwaUtils;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.reader.ReadingListHelper;
import org.mozilla.gecko.toolbar.BrowserToolbar.TabEditingState;
import org.mozilla.gecko.util.BitmapUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ShortcutUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.webapps.WebAppManifest;
import org.mozilla.gecko.widget.SiteLogins;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Log;

import static org.mozilla.gecko.toolbar.PageActionLayout.PageAction.UUID_PAGE_ACTION_PWA;

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

    private IconRequestBuilder mIconRequestBuilder;
    private Future<IconResponse> mRunningIconRequest;

    private boolean mHasFeeds;
    private String mManifestUrl;
    private WebAppManifest mWebAppManifest;
    private boolean mHasOpenSearch;
    private final SiteIdentity mSiteIdentity;
    private SiteLogins mSiteLogins;
    private BitmapDrawable mThumbnail;
    private volatile int mParentId;
    // Indicates the url was loaded from a source external to the app. This will be cleared
    // when the user explicitly loads a new url (e.g. clicking a link is not explicit).
    private final boolean mExternal;
    private boolean mBookmark;
    private int mFaviconLoadId;
    private String mContentType;
    private boolean mHasTouchListeners;
    private int mState;
    private Bitmap mThumbnailBitmap;
    private boolean mDesktopMode;
    private boolean mEnteringReaderMode;
    private final Context mAppContext;
    private ErrorType mErrorType = ErrorType.NONE;
    private volatile int mLoadProgress;
    private volatile int mRecordingCount;
    private volatile boolean mIsAudioPlaying;
    private volatile boolean mIsMediaPlaying;
    private String mMostRecentHomePanel;
    private boolean mShouldShowToolbarWithoutAnimationOnFirstSelection;

    /*
     * Bundle containing restore data for the panel referenced in mMostRecentHomePanel. This can be
     * e.g. the most recent folder for the bookmarks panel, or any other state that should be
     * persisted. This is then used e.g. when returning to homepanels via history.
     */
    private Bundle mMostRecentHomePanelData;

    private boolean mCanDoBack;
    private boolean mCanDoForward;

    private boolean mIsEditing;
    private final TabEditingState mEditingState = new TabEditingState();

    // Will be true when tab is loaded from cache while device was offline.
    private boolean mLoadedFromCache;

    public static final int STATE_DELAYED = 0;
    public static final int STATE_LOADING = 1;
    public static final int STATE_SUCCESS = 2;
    public static final int STATE_ERROR = 3;

    public static final int LOAD_PROGRESS_INIT = 10;
    public static final int LOAD_PROGRESS_START = 20;
    public static final int LOAD_PROGRESS_LOCATION_CHANGE = 60;
    public static final int LOAD_PROGRESS_LOADED = 80;
    public static final int LOAD_PROGRESS_STOP = 100;

    public WebAppManifest getWebAppManifest() {
        return mWebAppManifest;
    }

    public void setWebAppManifest(WebAppManifest mWebAppManifest) {
        this.mWebAppManifest = mWebAppManifest;
    }

    public enum ErrorType {
        CERT_ERROR,  // Pages with certificate problems
        BLOCKED,     // Pages blocked for phishing or malware warnings
        NET_ERROR,   // All other types of error
        NONE         // Non error pages
    }

    public Tab(Context context, int id, String url, boolean external, int parentId, String title) {
        mAppContext = context.getApplicationContext();
        mDB = BrowserDB.from(context);
        mId = id;
        mUrl = url;
        mBaseDomain = "";
        mUserRequested = "";
        mExternal = external;
        mParentId = parentId;
        mTitle = title == null ? "" : title;
        mSiteIdentity = new SiteIdentity();
        mContentType = "";
        mState = shouldShowProgress(url) ? STATE_LOADING : STATE_SUCCESS;
        mLoadProgress = LOAD_PROGRESS_INIT;
        mIconRequestBuilder = Icons.with(mAppContext).pageUrl(mUrl);

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

    /**
     * Updates the stored parent tab ID to a new value.
     * Note: Calling this directly from Java currently won't update the parent ID value
     * held by Gecko and the session store.
     *
     * @param parentId The ID of the tab to be set as new parent, or -1 for no parent.
     */
    public void setParentId(int parentId) {
        mParentId = parentId;
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
            boolean sizeChange = mThumbnailBitmap.getWidth() != width
                              || mThumbnailBitmap.getHeight() != height;
            if (sizeChange) {
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

    public String getManifestUrl() {
        return mManifestUrl;
    }

    public boolean hasOpenSearch() {
        return mHasOpenSearch;
    }

    public boolean hasLoadedFromCache() {
        return mLoadedFromCache;
    }

    public SiteIdentity getSiteIdentity() {
        return mSiteIdentity;
    }

    public void resetSiteIdentity() {
        if (mSiteIdentity != null) {
            mSiteIdentity.reset();
            Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.SECURITY_CHANGE);
        }
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

    public void setHasTouchListeners(boolean aValue) {
        mHasTouchListeners = aValue;
    }

    public boolean getHasTouchListeners() {
        return mHasTouchListeners;
    }

    public synchronized void addFavicon(@NonNull String faviconURL, int faviconSize, String mimeType) {
        mIconRequestBuilder
                .icon(IconDescriptor.createFavicon(faviconURL, faviconSize, mimeType))
                .deferBuild();
    }

    public synchronized void addTouchicon(@NonNull String iconUrl, int faviconSize, String mimeType) {
        mIconRequestBuilder
                .icon(IconDescriptor.createTouchicon(iconUrl, faviconSize, mimeType))
                .deferBuild();
    }

    public void loadFavicon() {
        // Static Favicons never change
        if (AboutPages.isBuiltinIconPage(mUrl) && mFavicon != null) {
            return;
        }

        mRunningIconRequest = mIconRequestBuilder
                .setPrivateMode(isPrivate())
                .build()
                .execute(new IconCallback() {
                    @Override
                    public void onIconResponse(IconResponse response) {
                        mFavicon = response.getBitmap();

                        Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.FAVICON);
                    }
                });
    }

    public synchronized void clearFavicon() {
        // Cancel any ongoing favicon load (if we never finished downloading the old favicon before
        // we changed page).
        if (mRunningIconRequest != null) {
            mRunningIconRequest.cancel(true);
        }

        // Keep the favicon unchanged while entering reader mode
        if (mEnteringReaderMode)
            return;

        mFavicon = null;
        mFaviconUrl = null;
    }

    public void setHasFeeds(boolean hasFeeds) {
        mHasFeeds = hasFeeds;
    }

    public void setManifestUrl(String manifestUrl) {
        mManifestUrl = manifestUrl;
        updatePageAction();
    }

    public void updatePageAction() {
        if (!ShortcutUtils.isPinShortcutSupported()) {
            return;
        }

        if (mManifestUrl != null) {
            showPwaBadge();

        } else {
            clearPwaPageAction();
        }
    }

    public void setHasOpenSearch(boolean hasOpenSearch) {
        mHasOpenSearch = hasOpenSearch;
    }

    public void setLoadedFromCache(boolean loadedFromCache) {
        mLoadedFromCache = loadedFromCache;
    }

    public void updateIdentityData(final GeckoBundle identityData) {
        mSiteIdentity.update(identityData);
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

        if (AboutPages.isAboutReader(url)) {
            ReadingListHelper.cacheReaderItem(pageUrl, mId, mAppContext);
            // defer bookmarking after completely added to cache.
        } else {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    mDB.addBookmark(getContentResolver(), mTitle, pageUrl);
                    Tabs.getInstance().notifyListeners(Tab.this, Tabs.TabEvents.BOOKMARK_ADDED);
                }
            });

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
        final GeckoBundle data = new GeckoBundle(1);
        data.putBoolean("bypassCache", bypassCache);
        EventDispatcher.getInstance().dispatch("Session:Reload", data);
    }

    // Our version of nsSHistory::GetCanGoBack
    public boolean canDoBack() {
        return mCanDoBack;
    }

    public boolean doBack() {
        if (!canDoBack())
            return false;

        EventDispatcher.getInstance().dispatch("Session:Back", null);
        return true;
    }

    public void doStop() {
        EventDispatcher.getInstance().dispatch("Session:Stop", null);
    }

    // Our version of nsSHistory::GetCanGoForward
    public boolean canDoForward() {
        return mCanDoForward;
    }

    public boolean doForward() {
        if (!canDoForward())
            return false;

        EventDispatcher.getInstance().dispatch("Session:Forward", null);
        return true;
    }

    void handleLocationChange(final GeckoBundle message) {
        final String uri = message.getString("uri");
        final String oldUrl = getURL();
        final boolean sameDocument = message.getBoolean("sameDocument");
        mEnteringReaderMode = ReaderModeUtils.isEnteringReaderMode(oldUrl, uri);
        handleButtonStateChange(message);

        if (!TextUtils.equals(oldUrl, uri)) {
            updateURL(uri);
            updateBookmark();
            if (!sameDocument) {
                // We can unconditionally clear the favicon and title here: we
                // already filtered both cases in which this was a (pseudo-)
                // spurious location change, so we're definitely loading a new
                // page.
                clearFavicon();

                // Start to build a new request to load a favicon.
                mIconRequestBuilder = Icons.with(mAppContext)
                        .pageUrl(uri);

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
        mBaseDomain = message.getString("baseDomain", "");

        setHasFeeds(false);
        setManifestUrl(null);
        setHasOpenSearch(false);
        mSiteIdentity.reset();
        setSiteLogins(null);
        setHasTouchListeners(false);
        setErrorType(ErrorType.NONE);
        setLoadProgressIfLoading(LOAD_PROGRESS_LOCATION_CHANGE);

        Tabs.getInstance().notifyListeners(this, Tabs.TabEvents.LOCATION_CHANGE, oldUrl);
    }

    void handleButtonStateChange(final GeckoBundle message) {
        mCanDoBack = message.getBoolean("canGoBack");
        mCanDoForward = message.getBoolean("canGoForward");
    }

    void handleButtonStateChange(boolean canGoBack, boolean canGoForward) {
        mCanDoBack = canGoBack;
        mCanDoForward = canGoForward;
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

    void handleLoadError() {
        setState(STATE_ERROR);
        setLoadProgress(LOAD_PROGRESS_LOADED);
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

    /**
     * The "MediaPlaying" is used for controling media control interface and
     * means the tab has playing media.
     *
     * @param isMediaPlaying the tab has any playing media or not
     */
    public void setIsMediaPlaying(boolean isMediaPlaying) {
        mIsMediaPlaying = isMediaPlaying;
    }

    public boolean isMediaPlaying() {
        return mIsMediaPlaying;
    }

    /**
     * The "AudioPlaying" is used for showing the tab sound indicator and means
     * the tab has playing media and the media is audible.
     *
     * @param isAudioPlaying the tab has any audible playing media or not
     */
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


    private void clearPwaPageAction() {
        GeckoBundle bundle = new GeckoBundle();
        bundle.putString("id", UUID_PAGE_ACTION_PWA);
        EventDispatcher.getInstance().dispatch("PageActions:Remove", bundle);
    }


    private void showPwaBadge() {
        if (PwaUtils.shouldAddPwaShortcut(this)) {
            GeckoBundle bundle = new GeckoBundle();
            bundle.putString("id", UUID_PAGE_ACTION_PWA);
            bundle.putString("title", mAppContext.getString(R.string.pwa_add_to_launcher_badge));
            bundle.putString("icon", "drawable://add_to_homescreen");
            bundle.putBoolean("important", true);
            EventDispatcher.getInstance().dispatch("PageActions:Add", bundle);
        } else {
            GeckoBundle bundle = new GeckoBundle();
            bundle.putString("id", UUID_PAGE_ACTION_PWA);
            EventDispatcher.getInstance().dispatch("PageActions:Remove", bundle);
        }
    }
}
