/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.concurrent.Future;

import android.content.Context;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.support.v4.widget.TextViewCompat;
import android.text.Spannable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.distribution.PartnerBookmarksProviderProxy;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

public class TwoLinePageRow extends ThemedLinearLayout
                            implements Tabs.OnTabsChangedListener {

    protected static final int NO_ICON = 0;

    private final ThemedTextView mTitle;
    private final ThemedTextView mUrl;
    private final ImageView mStatusIcon;

    private int mSwitchToTabIconId;

    private final FaviconView mFavicon;
    private Future<IconResponse> mOngoingIconLoad;

    private boolean mShowIcons;

    // The URL for the page corresponding to this view.
    private String mPageUrl;

    private boolean mHasReaderCacheItem;

    private TitleFormatter mTitleFormatter;

    public TwoLinePageRow(Context context) {
        this(context, null);
    }

    public TwoLinePageRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.two_line_page_row, this);

        mTitle = (ThemedTextView) findViewById(R.id.title);
        mUrl = (ThemedTextView) findViewById(R.id.url);
        mStatusIcon = (ImageView) findViewById(R.id.status_icon_bookmark);

        mSwitchToTabIconId = NO_ICON;
        mShowIcons = true;

        mFavicon = (FaviconView) findViewById(R.id.icon);
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
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final String data) {
        // Carefully check if this tab event is relevant to this row.
        final String pageUrl = mPageUrl;
        if (pageUrl == null) {
            return;
        }
        if (tab == null) {
            return;
        }

        // Return early if the page URL doesn't match the current tab URL,
        // or the old tab URL.
        // data is an empty String for ADDED/CLOSED, and contains the previous/old URL during
        // LOCATION_CHANGE (the new URL is retrieved using tab.getURL()).
        // tabURL and data may be about:reader URLs if the current or old tab page was a reader view
        // page, however pageUrl will always be a plain URL (i.e. we only add about:reader when opening
        // a reader view bookmark, at all other times it's a normal bookmark with normal URL).
        final String tabUrl = tab.getURL();
        if (!pageUrl.equals(ReaderModeUtils.stripAboutReaderUrl(tabUrl)) &&
            !pageUrl.equals(ReaderModeUtils.stripAboutReaderUrl(data))) {
            return;
        }

        // Note: we *might* need to update the display status (i.e. switch-to-tab icon/label) if
        // a matching tab has been opened/closed/switched to a different page. updateDisplayedUrl() will
        // determine the changes (if any) that actually need to be made. A tab change with a matching URL
        // does not imply that any changes are needed - e.g. if a given URL is already open in one tab, and
        // is also opened in a second tab, the switch-to-tab status doesn't change, closing 1 of 2 tabs with a URL
        // similarly doesn't change the switch-to-tab display, etc. (However closing the last tab for
        // a given URL does require a status change, as does opening the first tab with that URL.)
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

    private void setTitle(CharSequence text) {
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
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(mUrl, mSwitchToTabIconId, 0, 0, 0);
    }

    private void updateStatusIcon(boolean isBookmark, boolean isReaderItem) {
        if (isReaderItem) {
            mStatusIcon.setImageResource(R.drawable.status_icon_readercache);
        } else if (isBookmark) {
            mStatusIcon.setImageResource(R.drawable.star_blue);
        }

        if (mShowIcons && (isBookmark || isReaderItem)) {
            mStatusIcon.setVisibility(View.VISIBLE);
        } else if (mShowIcons) {
            // We use INVISIBLE to have consistent padding for our items. This means text/URLs
            // fade consistently in the same location, regardless of them being bookmarked.
            mStatusIcon.setVisibility(View.INVISIBLE);
        } else {
            mStatusIcon.setVisibility(View.GONE);
        }

    }

    /**
     * Stores the page URL, so that we can use it to replace "Switch to tab" if the open
     * tab changes or is closed.
     */
    private void updateDisplayedUrl(String url, boolean hasReaderCacheItem) {
        mPageUrl = url;
        mHasReaderCacheItem = hasReaderCacheItem;
        updateDisplayedUrl();
    }

    /**
     * Replaces the page URL with "Switch to tab" if there is already a tab open with that URL.
     * Only looks for tabs that are either private or non-private, depending on the current
     * selected tab.
     */
    protected void updateDisplayedUrl() {
        // We always want to display the underlying page url, however for readermode pages
        // we navigate to the about:reader equivalent, hence we need to use that url when finding
        // existing tabs
        final String navigationUrl = mHasReaderCacheItem ? ReaderModeUtils.getAboutReaderForUrl(mPageUrl) : mPageUrl;
        Tab tab = Tabs.getInstance().getFirstTabForUrl(navigationUrl, isCurrentTabInPrivateMode());


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
        update(title, url, 0, false);
    }

    protected void update(String title, String url, long bookmarkId, boolean hasReaderCacheItem) {
        if (mShowIcons) {
            // The bookmark id will be 0 (null in database) when the url
            // is not a bookmark and negative for 'fake' bookmarks.
            final boolean isBookmark = bookmarkId > 0;

            updateStatusIcon(isBookmark, hasReaderCacheItem);
        } else {
            updateStatusIcon(false, false);
        }

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        final String titleToShow = TextUtils.isEmpty(title) ? url : title;
        if (mTitleFormatter != null) {
            setTitle(mTitleFormatter.format(titleToShow));
        } else {
            setTitle(titleToShow);
        }

        // No point updating the below things if URL has not changed. Prevents evil Favicon flicker.
        if (url.equals(mPageUrl)) {
            return;
        }

        // Blank the Favicon, so we don't show the wrong Favicon if we scroll and miss DB.
        mFavicon.clearImage();

        if (mOngoingIconLoad != null) {
            mOngoingIconLoad.cancel(true);
        }

        // Displayed RecentTabsPanel URLs may refer to pages opened in reader mode, so we
        // remove the about:reader prefix to ensure the Favicon loads properly.
        final String pageURL = ReaderModeUtils.stripAboutReaderUrl(url);

        if (TextUtils.isEmpty(pageURL)) {
            // If url is empty, display the item as-is but do not load an icon if we do not have a page URL (bug 1310622)
        } else if (bookmarkId < BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START) {
            mOngoingIconLoad = Icons.with(getContext())
                    .pageUrl(pageURL)
                    .setPrivateMode(isCurrentTabInPrivateMode())
                    .skipNetwork()
                    .privileged(true)
                    .icon(IconDescriptor.createGenericIcon(
                            PartnerBookmarksProviderProxy.getUriForIcon(getContext(), bookmarkId).toString()))
                    .build()
                    .execute(mFavicon.createIconCallback());
        } else {
            mOngoingIconLoad = Icons.with(getContext())
                    .pageUrl(pageURL)
                    .setPrivateMode(isCurrentTabInPrivateMode())
                    .skipNetwork()
                    .build()
                    .execute(mFavicon.createIconCallback());

        }

        updateDisplayedUrl(url, hasReaderCacheItem);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        mTitle.setPrivateMode(isPrivate);
        mUrl.setPrivateMode(isPrivate);
    }

    /**
     * @return true if this view is shown inside a private tab, independent of whether
     * a private mode theme is applied via <code>setPrivateMode(true)</code>.
     */
    private boolean isCurrentTabInPrivateMode() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        return tab != null && tab.isPrivate();
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

        SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(getContext());
        final boolean hasReaderCacheItem = rch.isURLCached(url);

        update(title, url, bookmarkId, hasReaderCacheItem);
    }

    public void setTitleFormatter(TitleFormatter formatter) {
        mTitleFormatter = formatter;
    }

    // Use this interface to decorate content in title view.
    interface TitleFormatter {
        CharSequence format(@NonNull CharSequence title);
    }
}
