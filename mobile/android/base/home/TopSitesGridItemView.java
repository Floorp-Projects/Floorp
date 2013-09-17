/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A view that displays the thumbnail and the title/url for a top/pinned site.
 * If the title/url is longer than the width of the view, they are faded out.
 * If there is no valid url, a default string is shown at 50% opacity.
 * This is denoted by the empty state.
 */
public class TopSitesGridItemView extends LinearLayout {
    private static final String LOGTAG = "GeckoTopSitesGridItemView";

    // Empty state, to denote there is no valid url.
    private static final int[] STATE_EMPTY = { android.R.attr.state_empty };

    // Child views.
    private final TextView mTitleView;
    private final ImageView mThumbnailView;

    // Data backing this view.
    private String mTitle;
    private String mUrl;

    // Pinned state.
    private boolean mIsPinned = false;

    // Empty state.
    private boolean mIsEmpty = true;

    public TopSitesGridItemView(Context context) {
        this(context, null);
    }

    public TopSitesGridItemView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.topSitesGridItemViewStyle);
    }

    public TopSitesGridItemView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        LayoutInflater.from(context).inflate(R.layout.top_sites_grid_item_view, this);

        mTitleView = (TextView) findViewById(R.id.title);
        mThumbnailView = (ImageView) findViewById(R.id.thumbnail);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mIsEmpty) {
            mergeDrawableStates(drawableState, STATE_EMPTY);
        }

        return drawableState;
    }

    /**
     * @return The title shown by this view.
     */
    public String getTitle() {
        return (!TextUtils.isEmpty(mTitle) ? mTitle : mUrl);
    }

    /**
     * @return The url shown by this view.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @return true, if this view is pinned, false otherwise.
     */
    public boolean isPinned() {
        return mIsPinned;
    }

    /**
     * @param title The title for this view.
     */
    public void setTitle(String title) {
        if (mTitle != null && mTitle.equals(title)) {
            return;
        }

        mTitle = title;
        updateTitleView();
    }

    /**
     * @param url The url for this view.
     */
    public void setUrl(String url) {
        if (mUrl != null && mUrl.equals(url)) {
            return;
        }

        mUrl = url;
        updateTitleView();
    }

    /**
     * @param pinned The pinned state of this view.
     */
    public void setPinned(boolean pinned) {
        mIsPinned = pinned;
        mTitleView.setCompoundDrawablesWithIntrinsicBounds(pinned ? R.drawable.pin : 0, 0, 0, 0);
    }

    /**
     * Display the thumbnail from a resource.
     *
     * @param resId Resource ID of the drawable to show.
     */
    public void displayThumbnail(int resId) {
        mThumbnailView.setScaleType(ScaleType.CENTER);
        mThumbnailView.setImageResource(resId);
        mThumbnailView.setBackgroundColor(0x0);
    }

    /**
     * Display the thumbnail from a bitmap.
     *
     * @param thumbnail The bitmap to show as thumbnail.
     */
    public void displayThumbnail(Bitmap thumbnail) {
        if (thumbnail == null) {
            // Show a favicon based view instead.
            displayThumbnail(R.drawable.favicon);
            return;
        }

        mThumbnailView.setScaleType(ScaleType.CENTER_CROP);
        mThumbnailView.setImageBitmap(thumbnail);
        mThumbnailView.setBackgroundDrawable(null);
    }

    /**
     * Display the thumbnail from a favicon.
     *
     * @param favicon The favicon to show as thumbnail.
     */
    public void displayFavicon(Bitmap favicon) {
        if (favicon == null) {
            // Should show default favicon.
            displayThumbnail(R.drawable.favicon);
            return;
        }

        mThumbnailView.setScaleType(ScaleType.CENTER);
        mThumbnailView.setImageBitmap(favicon);
        mThumbnailView.setBackgroundColor(Favicons.getFaviconColor(favicon, mUrl));
    }

    /**
     * Update the title shown by this view. If both title and url
     * are empty, mark the state as STATE_EMPTY and show a default text.
     */
    private void updateTitleView() {
        String title = getTitle();
        if (!TextUtils.isEmpty(title)) {
            mTitleView.setText(title);
            mIsEmpty = false;
        } else {
            mTitleView.setText(R.string.bookmark_add);
            mIsEmpty = true;
        }

        // Refresh for state change.
        refreshDrawableState();
    }
}
