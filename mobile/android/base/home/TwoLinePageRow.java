/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import org.mozilla.gecko.widget.FaviconView;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

public class TwoLinePageRow extends LinearLayout
                            implements Tabs.OnTabsChangedListener {
    private static final int NO_ICON = 0;

    private final TextView mTitle;
    private final TextView mUrl;
    private final FaviconView mFavicon;

    private int mUrlIconId;
    private int mBookmarkIconId;
    private boolean mShowIcons;

    // The URL for the page corresponding to this view.
    private String mPageUrl;

    private LoadFaviconTask mLoadFaviconTask;

    public TwoLinePageRow(Context context) {
        this(context, null);
    }

    public TwoLinePageRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        mUrlIconId = NO_ICON;
        mBookmarkIconId = NO_ICON;
        mShowIcons = true;

        LayoutInflater.from(context).inflate(R.layout.two_line_page_row, this);
        mTitle = (TextView) findViewById(R.id.title);
        mUrl = (TextView) findViewById(R.id.url);
        mFavicon = (FaviconView) findViewById(R.id.favicon);
    }

    @Override
    protected void onAttachedToWindow() {
        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    protected void onDetachedFromWindow() {
        // Delay removing the listener to avoid modifying mTabsChangedListeners
        // while notifyListeners is iterating through the array.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Tabs.unregisterOnTabsChangedListener(TwoLinePageRow.this);
            }
        });

        cancelLoadFaviconTask();
    }

    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final Object data) {
        switch(msg) {
            case ADDED:
            case CLOSED:
            case LOCATION_CHANGE:
                updateDisplayedUrl();
                break;
        }
    }

    private void setTitle(String title) {
        mTitle.setText(title);
    }

    private void setUrl(String url) {
        mUrl.setText(url);
    }

    private void setUrl(int stringId) {
        mUrl.setText(stringId);
    }

    private void setUrlIcon(int urlIconId) {
        if (mUrlIconId == urlIconId) {
            return;
        }

        mUrlIconId = urlIconId;
        mUrl.setCompoundDrawablesWithIntrinsicBounds(mUrlIconId, 0, mBookmarkIconId, 0);
    }

    private void setFaviconWithUrl(Bitmap favicon, String url) {
        mFavicon.updateImage(favicon, url);
    }

    private void setBookmarkIcon(int bookmarkIconId) {
        if (mBookmarkIconId == bookmarkIconId) {
            return;
        }

        mBookmarkIconId = bookmarkIconId;
        mUrl.setCompoundDrawablesWithIntrinsicBounds(mUrlIconId, 0, mBookmarkIconId, 0);
    }

    /**
     * Stores the page URL, so that we can use it to replace "Switch to tab" if the open
     * tab changes or is closed.
     */
    private void updateDisplayedUrl(String url) {
        mPageUrl = url;
        updateDisplayedUrl();
    }

    /**
     * Cancels any pending favicon loading task associated with this view.
     */
    private void cancelLoadFaviconTask() {
        if (mLoadFaviconTask != null) {
            mLoadFaviconTask.cancel(true);
            mLoadFaviconTask = null;
        }
    }

    /**
     * Replaces the page URL with "Switch to tab" if there is already a tab open with that URL.
     */
    private void updateDisplayedUrl() {
        int tabId = Tabs.getInstance().getTabIdForUrl(mPageUrl);
        if (!mShowIcons || tabId < 0) {
            setUrl(mPageUrl);
            setUrlIcon(NO_ICON);
        } else {
            setUrl(R.string.switch_to_tab);
            setUrlIcon(R.drawable.ic_url_bar_tab);
        }
    }

    public void setShowIcons(boolean showIcons) {
        mShowIcons = showIcons;
    }

    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        final String title = cursor.getString(titleIndex);

        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        final String url = cursor.getString(urlIndex);

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        setTitle(TextUtils.isEmpty(title) ? url : title);

        // No need to do extra work if the URL associated with this view
        // hasn't changed.
        if (TextUtils.equals(mPageUrl, url)) {
            return;
        }

        updateDisplayedUrl(url);
        cancelLoadFaviconTask();

        // First, try to find the favicon in the memory cache. If it's not
        // cached yet, try to load it from the database, off main thread.
        final Bitmap favicon = Favicons.getFaviconFromMemCache(url);
        if (favicon != null) {
            setFaviconWithUrl(favicon, url);
        } else {
            mLoadFaviconTask = new LoadFaviconTask(url);

            // Try to use a thread pool instead of serial execution of tasks
            // to add more throughput to the favicon loading routines.
            if (Build.VERSION.SDK_INT >= 11) {
                mLoadFaviconTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            } else {
                mLoadFaviconTask.execute();
            }
        }

        // Don't show bookmark/reading list icon, if not needed.
        if (!mShowIcons) {
            return;
        }

        final int bookmarkIdIndex = cursor.getColumnIndex(Combined.BOOKMARK_ID);
        if (bookmarkIdIndex != -1) {
            final long bookmarkId = cursor.getLong(bookmarkIdIndex);
            final int displayIndex = cursor.getColumnIndex(Combined.DISPLAY);

            final int display;
            if (displayIndex != -1) {
                display = cursor.getInt(displayIndex);
            } else {
                display = Combined.DISPLAY_NORMAL;
            }

            // The bookmark id will be 0 (null in database) when the url
            // is not a bookmark.
            if (bookmarkId == 0) {
                setBookmarkIcon(NO_ICON);
            } else if (display == Combined.DISPLAY_READER) {
                setBookmarkIcon(R.drawable.ic_url_bar_reader);
            } else {
                setBookmarkIcon(R.drawable.ic_url_bar_star);
            }
        } else {
            setBookmarkIcon(NO_ICON);
        }
    }

    private class LoadFaviconTask extends AsyncTask<Void, Void, Bitmap> {
        private final String mUrl;

        public LoadFaviconTask(String url) {
            mUrl = url;
        }

        @Override
        public Bitmap doInBackground(Void... params) {
            Bitmap favicon = Favicons.getFaviconFromMemCache(mUrl);
            if (favicon == null) {
                final ContentResolver cr = getContext().getContentResolver();

                final Bitmap faviconFromDb = BrowserDB.getFaviconForUrl(cr, mUrl);
                if (faviconFromDb != null) {
                    favicon = Favicons.scaleImage(faviconFromDb);
                    Favicons.putFaviconInMemCache(mUrl, favicon);
                }
            }

            return favicon;
        }

        @Override
        public void onPostExecute(Bitmap favicon) {
            if (TextUtils.equals(mPageUrl, mUrl)) {
                setFaviconWithUrl(favicon, mUrl);
            }

            mLoadFaviconTask = null;
        }
    }
}
