/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.db.BrowserContract.TopSites;
import org.mozilla.gecko.db.TopSitesCursorWrapper;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.StringUtils;

import android.content.Context;
import android.content.res.TypedArray;
import android.database.Cursor;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.GridView;

/**
 * A grid view of top and pinned sites.
 * Each cell in the grid is a TopSitesGridItemView.
 */
public class TopSitesGridView extends GridView {
    private static final String LOGTAG = "GeckoTopSitesGridView";

    // Listener for editing pinned sites.
    public static interface OnEditPinnedSiteListener {
        public void onEditPinnedSite(int position, String searchTerm);
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

    // Edit pinned site listener.
    private OnEditPinnedSiteListener mEditPinnedSiteListener;

    // Context menu info.
    private TopSitesGridContextMenuInfo mContextMenuInfo;

    // Whether we're handling focus changes or not. This is used
    // to avoid infinite re-layouts when using this GridView as
    // a ListView header view (see bug 918044).
    private boolean mIsHandlingFocusChange;

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

        mIsHandlingFocusChange = false;
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
                TopSitesGridItemView item = (TopSitesGridItemView) view;

                // Decode "user-entered" URLs before loading them.
                String url = StringUtils.decodeUserEnteredUrl(item.getUrl());
                int type = item.getType();

                // If the url is empty, the user can pin a site.
                // If not, navigate to the page given by the url.
                if (type != TopSites.TYPE_BLANK) {
                    if (mUrlOpenListener != null) {
                        final TelemetryContract.Method method;
                        if (type == TopSites.TYPE_SUGGESTED) {
                            method = TelemetryContract.Method.SUGGESTION;
                        } else {
                            method = TelemetryContract.Method.GRID_ITEM;
                        }
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, method, Integer.toString(position));

                        mUrlOpenListener.onUrlOpen(url, EnumSet.noneOf(OnUrlOpenListener.Flags.class));
                    }
                } else {
                    if (mEditPinnedSiteListener != null) {
                        mEditPinnedSiteListener.onEditPinnedSite(position, "");
                    }
                }
            }
        });

        setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                Cursor cursor = (Cursor) parent.getItemAtPosition(position);

                TopSitesGridItemView item = (TopSitesGridItemView) view;
                if (cursor == null || item.getType() == TopSites.TYPE_BLANK) {
                    mContextMenuInfo = null;
                    return false;
                }

                mContextMenuInfo = new TopSitesGridContextMenuInfo(view, position, id);
                updateContextMenuFromCursor(mContextMenuInfo, cursor);
                return showContextMenuForChild(TopSitesGridView.this);
            }
        });
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mUrlOpenListener = null;
        mEditPinnedSiteListener = null;
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        mIsHandlingFocusChange = true;
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        mIsHandlingFocusChange = false;
    }

    @Override
    public void requestLayout() {
        if (!mIsHandlingFocusChange) {
            super.requestLayout();
        }
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

        final int columnWidth = getColumnWidth();

        // Get the first child from the adapter.
        final TopSitesGridItemView child = new TopSitesGridItemView(getContext());

        // Set a default LayoutParams on the child, if it doesn't have one on its own.
        AbsListView.LayoutParams params = (AbsListView.LayoutParams) child.getLayoutParams();
        if (params == null) {
            params = new AbsListView.LayoutParams(AbsListView.LayoutParams.WRAP_CONTENT,
                                                  AbsListView.LayoutParams.WRAP_CONTENT);
            child.setLayoutParams(params);
        }

        // Measure the exact width of the child, and the height based on the width.
        // Note: the child (and TopSitesThumbnailView) takes care of calculating its height.
        int childWidthSpec = MeasureSpec.makeMeasureSpec(columnWidth, MeasureSpec.EXACTLY);
        int childHeightSpec = MeasureSpec.makeMeasureSpec(0,  MeasureSpec.UNSPECIFIED);
        child.measure(childWidthSpec, childHeightSpec);
        final int childHeight = child.getMeasuredHeight();

        // This is the maximum width of the contents of each child in the grid.
        // Use this as the target width for thumbnails.
        final int thumbnailWidth = child.getMeasuredWidth() - child.getPaddingLeft() - child.getPaddingRight();
        ThumbnailHelper.getInstance().setThumbnailWidth(thumbnailWidth);

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

    /*
     * Update the fields of a TopSitesGridContextMenuInfo object
     * from a cursor.
     *
     * @param  info    context menu info object to be updated
     * @param  cursor  used to update the context menu info object
     */
    private void updateContextMenuFromCursor(TopSitesGridContextMenuInfo info, Cursor cursor) {
        info.url = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.URL));
        info.title = cursor.getString(cursor.getColumnIndexOrThrow(TopSites.TITLE));
        info.type = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.TYPE));
        info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(TopSites.HISTORY_ID));
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
     * Set an edit pinned site listener to be used by this view.
     *
     * @param listener An edit pinned site listener for this view.
     */
    public void setOnEditPinnedSiteListener(final OnEditPinnedSiteListener listener) {
        mEditPinnedSiteListener = listener;
    }

    /**
     * Stores information regarding the creation of the context menu for a GridView item.
     */
    public static class TopSitesGridContextMenuInfo extends HomeContextMenuInfo {
        public int type = -1;

        public TopSitesGridContextMenuInfo(View targetView, int position, long id) {
            super(targetView, position, id);
        }
    }
}
