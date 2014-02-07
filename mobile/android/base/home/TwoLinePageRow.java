/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.util.Log;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.FaviconView;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.TextView;

import java.lang.ref.WeakReference;

public class TwoLinePageRow extends TwoLineRow
                            implements Tabs.OnTabsChangedListener {

    private static final int NO_ICON = 0;

    private final TextView mDescription;
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

        mShowIcons = true;

        mDescription = (TextView) findViewById(R.id.description);
        mSwitchToTabIconId = NO_ICON;
        mPageTypeIconId = NO_ICON;

        mFavicon = (FaviconView) findViewById(R.id.icon);
        mFaviconListener = new UpdateViewFaviconLoadedListener(mFavicon);
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

    private void setSwitchToTabIcon(int iconId) {
        if (mSwitchToTabIconId == iconId) {
            return;
        }

        mSwitchToTabIconId = iconId;
        mDescription.setCompoundDrawablesWithIntrinsicBounds(mSwitchToTabIconId, 0, mPageTypeIconId, 0);
    }

    private void setPageTypeIcon(int iconId) {
        if (mPageTypeIconId == iconId) {
            return;
        }

        mPageTypeIconId = iconId;
        mDescription.setCompoundDrawablesWithIntrinsicBounds(mSwitchToTabIconId, 0, mPageTypeIconId, 0);
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
    private void updateDisplayedUrl() {
        boolean isPrivate = Tabs.getInstance().getSelectedTab().isPrivate();
        Tab tab = Tabs.getInstance().getFirstTabForUrl(mPageUrl, isPrivate);
        if (!mShowIcons || tab == null) {
            setDescription(mPageUrl);
            setSwitchToTabIcon(NO_ICON);
        } else {
            setDescription(R.string.switch_to_tab);
            setSwitchToTabIcon(R.drawable.ic_url_bar_tab);
        }
    }

    public void setShowIcons(boolean showIcons) {
        mShowIcons = showIcons;
    }

    @Override
    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        final String title = cursor.getString(titleIndex);

        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        final String url = cursor.getString(urlIndex);

        if (mShowIcons) {
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
                    setPageTypeIcon(NO_ICON);
                } else if (display == Combined.DISPLAY_READER) {
                    setPageTypeIcon(R.drawable.ic_url_bar_reader);
                } else {
                    setPageTypeIcon(R.drawable.ic_url_bar_star);
                }
            } else {
                setPageTypeIcon(NO_ICON);
            }
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
        mLoadFaviconJobId = Favicons.getSizedFaviconForPageFromLocal(url, mFaviconListener);

        updateDisplayedUrl(url);
    }
}
