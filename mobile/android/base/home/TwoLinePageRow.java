/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.lang.ref.WeakReference;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ReaderModeUtils;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.widget.FaviconView;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

public class TwoLinePageRow extends LinearLayout
                            implements Tabs.OnTabsChangedListener {

    protected static final int NO_ICON = 0;

    private final TextView mTitle;
    private final TextView mUrl;

    private int mSwitchToTabIconId;
    private int mPageTypeIconId;

    private final FaviconView mFavicon;

    private boolean mShowIcons;
    private int mLoadFaviconJobId = Favicons.NOT_LOADING;

    // Only holds a reference to the FaviconView itself, so if the row gets
    // discarded while a task is outstanding, we'll leak less memory.
    private static class UpdateViewFaviconLoadedListener implements OnFaviconLoadedListener {
        private final WeakReference<FaviconView> view;
        public UpdateViewFaviconLoadedListener(FaviconView view) {
            this.view = new WeakReference<FaviconView>(view);
        }

        /**
         * Update this row's favicon.
         * <p>
         * This method is always invoked on the UI thread.
         */
        @Override
        public void onFaviconLoaded(String url, String faviconURL, Bitmap favicon) {
            FaviconView v = view.get();
            if (v == null) {
                // Guess we stuck around after the TwoLinePageRow went away.
                return;
            }

            if (favicon == null) {
                v.showDefaultFavicon();
                return;
            }

            v.updateImage(favicon, faviconURL);
        }
    }

    // Listener for handling Favicon loads.
    private final OnFaviconLoadedListener mFaviconListener;

    // The URL for the page corresponding to this view.
    private String mPageUrl;

    public TwoLinePageRow(Context context) {
        this(context, null);
    }

    public TwoLinePageRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.two_line_page_row, this);
        mTitle = (TextView) findViewById(R.id.title);
        mUrl = (TextView) findViewById(R.id.url);

        mSwitchToTabIconId = NO_ICON;
        mPageTypeIconId = NO_ICON;
        mShowIcons = true;

        mFavicon = (FaviconView) findViewById(R.id.icon);
        mFaviconListener = new UpdateViewFaviconLoadedListener(mFavicon);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        // Tabs' listener array is safe to modify during use: its
        // iteration pattern is based on snapshots.
        Tabs.unregisterOnTabsChangedListener(this);
    }

    /**
     * Update the row in response to a tab change event.
     * <p>
     * This method is always invoked on the UI thread.
     */
    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final Object data) {
        // Carefully check if this tab event is relevant to this row.
        final String pageUrl = mPageUrl;
        if (pageUrl == null) {
            return;
        }
        final String tabUrl;
        if (tab == null) {
            return;
        }
        tabUrl = tab.getURL();
        if (!pageUrl.equals(tabUrl)) {
            return;
        }

        switch (msg) {
            case ADDED:
            case CLOSED:
            case LOCATION_CHANGE:
                updateDisplayedUrl();
                break;
            default:
                break;
        }
    }

    private void setTitle(String text) {
        mTitle.setText(text);
    }

    protected void setUrl(String text) {
        mUrl.setText(text);
    }

    protected void setUrl(int stringId) {
        mUrl.setText(stringId);
    }

    protected String getUrl() {
        return mPageUrl;
    }

    protected void setSwitchToTabIcon(int iconId) {
        if (mSwitchToTabIconId == iconId) {
            return;
        }

        mSwitchToTabIconId = iconId;
        mUrl.setCompoundDrawablesWithIntrinsicBounds(mSwitchToTabIconId, 0, mPageTypeIconId, 0);
    }

    private void setPageTypeIcon(int iconId) {
        if (mPageTypeIconId == iconId) {
            return;
        }

        mPageTypeIconId = iconId;
        mUrl.setCompoundDrawablesWithIntrinsicBounds(mSwitchToTabIconId, 0, mPageTypeIconId, 0);
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
     * Replaces the page URL with "Switch to tab" if there is already a tab open with that URL.
     * Only looks for tabs that are either private or non-private, depending on the current
     * selected tab.
     */
    protected void updateDisplayedUrl() {
        boolean isPrivate = Tabs.getInstance().getSelectedTab().isPrivate();
        Tab tab = Tabs.getInstance().getFirstTabForUrl(mPageUrl, isPrivate);
        if (!mShowIcons || tab == null) {
            setUrl(mPageUrl);
            setSwitchToTabIcon(NO_ICON);
        } else {
            setUrl(R.string.switch_to_tab);
            setSwitchToTabIcon(R.drawable.ic_url_bar_tab);
        }
    }

    public void setShowIcons(boolean showIcons) {
        mShowIcons = showIcons;
    }

    /**
     * Update the data displayed by this row.
     * <p>
     * This method must be invoked on the UI thread.
     *
     * @param title to display.
     * @param url to display.
     */
    public void update(String title, String url) {
        update(title, url, 0);
    }

    protected void update(String title, String url, long bookmarkId) {
        if (mShowIcons) {
            // The bookmark id will be 0 (null in database) when the url
            // is not a bookmark.
            if (bookmarkId == 0) {
                setPageTypeIcon(NO_ICON);
            } else {
                setPageTypeIcon(R.drawable.ic_url_bar_star);
            }
        } else {
            setPageTypeIcon(NO_ICON);
        }

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        setTitle(TextUtils.isEmpty(title) ? url : title);

        // No point updating the below things if URL has not changed. Prevents evil Favicon flicker.
        if (url.equals(mPageUrl)) {
            return;
        }

        // Blank the Favicon, so we don't show the wrong Favicon if we scroll and miss DB.
        mFavicon.clearImage();
        Favicons.cancelFaviconLoad(mLoadFaviconJobId);

        // Displayed RecentTabsPanel URLs may refer to pages opened in reader mode, so we
        // remove the about:reader prefix to ensure the Favicon loads properly.
        final String pageURL = AboutPages.isAboutReader(url) ?
            ReaderModeUtils.getUrlFromAboutReader(url) : url;
        mLoadFaviconJobId = Favicons.getSizedFaviconForPageFromLocal(getContext(), pageURL, mFaviconListener);

        updateDisplayedUrl(url);
    }

    /**
     * Update the data displayed by this row.
     * <p>
     * This method must be invoked on the UI thread.
     *
     * @param cursor to extract data from.
     */
    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        final String title = cursor.getString(titleIndex);

        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        final String url = cursor.getString(urlIndex);

        final long bookmarkId;
        final int bookmarkIdIndex = cursor.getColumnIndex(Combined.BOOKMARK_ID);
        if (bookmarkIdIndex != -1) {
            bookmarkId = cursor.getLong(bookmarkIdIndex);
        } else {
            bookmarkId = 0;
        }

        update(title, url, bookmarkId);
    }
}
