/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.db.BrowserDB.TopSitesCursorWrapper;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.content.res.TypedArray;
import android.database.Cursor;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.GridView;

import java.util.EnumSet;

/**
 * A grid view of top and pinned sites.
 * Each cell in the grid is a TopSitesGridItemView.
 */
public class TopSitesGridView extends GridView {
    private static final String LOGTAG = "GeckoTopSitesGridView";

    // Listener for pinning sites.
    public static interface OnPinSiteListener {
        public void onPinSite(int position);
    }

    // Max number of top sites that needs to be shown.
    private final int mMaxSites;

    // Number of columns to show.
    private final int mNumColumns;

    // Horizontal spacing in between the rows.
    private final int mHorizontalSpacing;

    // Vertical spacing in between the rows.
    private final int mVerticalSpacing;

    // Measured width of this view.
    private int mMeasuredWidth;

    // Measured height of this view.
    private int mMeasuredHeight;

    // On URL open listener.
    private OnUrlOpenListener mUrlOpenListener;

    // Pin site listener.
    private OnPinSiteListener mPinSiteListener;

    // Context menu info.
    private TopSitesGridContextMenuInfo mContextMenuInfo;

    public TopSitesGridView(Context context) {
        this(context, null);
    }

    public TopSitesGridView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.topSitesGridViewStyle);
    }

    public TopSitesGridView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mMaxSites = getResources().getInteger(R.integer.number_of_top_sites);
        mNumColumns = getResources().getInteger(R.integer.number_of_top_sites_cols);
        setNumColumns(mNumColumns);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TopSitesGridView, defStyle, 0);
        mHorizontalSpacing = a.getDimensionPixelOffset(R.styleable.TopSitesGridView_android_horizontalSpacing, 0x00);
        mVerticalSpacing = a.getDimensionPixelOffset(R.styleable.TopSitesGridView_android_verticalSpacing, 0x00);
        a.recycle();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                TopSitesGridItemView row = (TopSitesGridItemView) view;
                String url = row.getUrl();

                // If the url is empty, the user can pin a site.
                // If not, navigate to the page given by the url.
                if (!TextUtils.isEmpty(url)) {
                    if (mUrlOpenListener != null) {
                        mUrlOpenListener.onUrlOpen(url, EnumSet.noneOf(OnUrlOpenListener.Flags.class));
                    }
                } else {
                    if (mPinSiteListener != null) {
                        mPinSiteListener.onPinSite(position);
                    }
                }
            }
        });

        setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                Cursor cursor = (Cursor) parent.getItemAtPosition(position);
                mContextMenuInfo = new TopSitesGridContextMenuInfo(view, position, id, cursor);
                return showContextMenuForChild(TopSitesGridView.this);
            }
        });
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mUrlOpenListener = null;
        mPinSiteListener = null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int getColumnWidth() {
        // This method will be called from onMeasure() too.
        // It's better to use getMeasuredWidth(), as it is safe in this case.
        final int totalHorizontalSpacing = mNumColumns > 0 ? (mNumColumns - 1) * mHorizontalSpacing : 0;
        return (getMeasuredWidth() - getPaddingLeft() - getPaddingRight() - totalHorizontalSpacing) / mNumColumns;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Sets the padding for this view.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        final int measuredWidth = getMeasuredWidth();
        if (measuredWidth == mMeasuredWidth) {
            // Return the cached values as the width is the same.
            setMeasuredDimension(mMeasuredWidth, mMeasuredHeight);
            return;
        }

        final int childWidth = getColumnWidth();

        // Set the column width as the thumbnail width.
        ThumbnailHelper.getInstance().setThumbnailWidth(childWidth);

        // Get the first child from the adapter.
        final View child = new TopSitesGridItemView(getContext());

        // Set a default LayoutParams on the child, if it doesn't have one on its own.
        AbsListView.LayoutParams params = (AbsListView.LayoutParams) child.getLayoutParams();
        if (params == null) {
            params = new AbsListView.LayoutParams(AbsListView.LayoutParams.WRAP_CONTENT,
                                                  AbsListView.LayoutParams.WRAP_CONTENT);
            child.setLayoutParams(params);
        }

        // Measure the exact width of the child, and the height based on the width.
        // Note: the child (and TopSitesThumbnailView) takes care of calculating its height.
        int childWidthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY);
        int childHeightSpec = MeasureSpec.makeMeasureSpec(0,  MeasureSpec.UNSPECIFIED);
        child.measure(childWidthSpec, childHeightSpec);
        final int childHeight = child.getMeasuredHeight();

        // Number of rows required to show these top sites.
        final int rows = (int) Math.ceil((double) mMaxSites / mNumColumns);
        final int childrenHeight = childHeight * rows;
        final int totalVerticalSpacing = rows > 0 ? (rows - 1) * mVerticalSpacing : 0;

        // Total height of this view.
        final int measuredHeight = childrenHeight + getPaddingTop() + getPaddingBottom() + totalVerticalSpacing;
        setMeasuredDimension(measuredWidth, measuredHeight);
        mMeasuredWidth = measuredWidth;
        mMeasuredHeight = measuredHeight;
    }

    @Override
    public ContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

    /**
     * Set an url open listener to be used by this view.
     *
     * @param listener An url open listener for this view.
     */
    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    /**
     * Set a pin site listener to be used by this view.
     *
     * @param listener A pin site listener for this view.
     */
    public void setOnPinSiteListener(OnPinSiteListener listener) {
        mPinSiteListener = listener;
    }

    /**
     * A ContextMenuInfo for TopBoomarksView that adds details from the cursor.
     */
    public static class TopSitesGridContextMenuInfo extends AdapterContextMenuInfo {

        // URL to Title replacement regex.
        private static final String REGEX_URL_TO_TITLE = "^([a-z]+://)?(www\\.)?";

        public String url;
        public String title;
        public boolean isPinned;

        public TopSitesGridContextMenuInfo(View targetView, int position, long id, Cursor cursor) {
            super(targetView, position, id);

            if (cursor == null) {
                return;
            }

            url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
            isPinned = ((TopSitesCursorWrapper) cursor).isPinned();
        }

        public String getDisplayTitle() {
            return TextUtils.isEmpty(title) ? url.replaceAll(REGEX_URL_TO_TITLE, "") : title;
        }
    }
}
