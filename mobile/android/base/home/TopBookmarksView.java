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
import android.database.Cursor;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.GridView;

/**
 * A grid view of top bookmarks and pinned tabs.
 * Each cell in the grid is a TopBookmarkItemView.
 */
public class TopBookmarksView extends GridView {
    private static final String LOGTAG = "GeckoTopBookmarksView";

    // Listener for pinning bookmarks.
    public static interface OnPinBookmarkListener {
        public void onPinBookmark(int position);
    }

    // Max number of bookmarks that needs to be shown.
    private final int mMaxBookmarks;

    // Number of columns to show.
    private final int mNumColumns;

    // On URL open listener.
    private OnUrlOpenListener mUrlOpenListener;

    // Pin bookmark listener.
    private OnPinBookmarkListener mPinBookmarkListener;

    // Context menu info.
    private TopBookmarksContextMenuInfo mContextMenuInfo;

    public TopBookmarksView(Context context) {
        this(context, null);
    }

    public TopBookmarksView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.topBookmarksViewStyle);
    }

    public TopBookmarksView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mMaxBookmarks = getResources().getInteger(R.integer.number_of_top_sites);
        mNumColumns = getResources().getInteger(R.integer.number_of_top_sites_cols);
        setNumColumns(mNumColumns);
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
                TopBookmarkItemView row = (TopBookmarkItemView) view;
                String url = row.getUrl();

                // If the url is empty, the user can pin a bookmark.
                // If not, navigate to the page given by the url.
                if (!TextUtils.isEmpty(url)) {
                    if (mUrlOpenListener != null) {
                        mUrlOpenListener.onUrlOpen(url);
                    }
                } else {
                    if (mPinBookmarkListener != null) {
                        mPinBookmarkListener.onPinBookmark(position);
                    }
                }
            }
        });

        setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                Cursor cursor = (Cursor) parent.getItemAtPosition(position);
                mContextMenuInfo = new TopBookmarksContextMenuInfo(view, position, id, cursor);
                return showContextMenuForChild(TopBookmarksView.this);
            }
        });
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mUrlOpenListener = null;
        mPinBookmarkListener = null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int getColumnWidth() {
        // This method will be called from onMeasure() too.
        // It's better to use getMeasuredWidth(), as it is safe in this case.
        return (getMeasuredWidth() - getPaddingLeft() - getPaddingRight()) / mNumColumns;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Sets the padding for this view.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        final int measuredWidth = getMeasuredWidth();
        final int childWidth = getColumnWidth();
        int childHeight = 0;

        // Set the column width as the thumbnail width.
        ThumbnailHelper.getInstance().setThumbnailWidth(childWidth);

        // If there's an adapter, use it to calculate the height of this view.
        final TopBookmarksAdapter adapter = (TopBookmarksAdapter) getAdapter();
        final int count = (adapter == null ? 0 : adapter.getCount());

        if (adapter != null && count > 0) {
            // Get the first child from the adapter.
            final View child = adapter.getView(0, null, this);
            if (child != null) {
                // Set a default LayoutParams on the child, if it doesn't have one on its own.
                AbsListView.LayoutParams params = (AbsListView.LayoutParams) child.getLayoutParams();
                if (params == null) {
                    params = new AbsListView.LayoutParams(AbsListView.LayoutParams.WRAP_CONTENT,
                                                          AbsListView.LayoutParams.WRAP_CONTENT);
                    child.setLayoutParams(params);
                }

                // Measure the exact width of the child, and the height based on the width.
                // Note: the child (and BookmarkThumbnailView) takes care of calculating its height.
                int childWidthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY);
                int childHeightSpec = MeasureSpec.makeMeasureSpec(0,  MeasureSpec.UNSPECIFIED);
                child.measure(childWidthSpec, childHeightSpec);
                childHeight = child.getMeasuredHeight();
            }
        }

        // Find the minimum of bookmarks we need to show, and the one given by the cursor.
        final int total = Math.min(count > 0 ? count : Integer.MAX_VALUE, mMaxBookmarks);

        // Number of rows required to show these bookmarks.
        final int rows = (int) Math.ceil((double) total / mNumColumns);
        final int childrenHeight = childHeight * rows;

        // Total height of this view.
        final int measuredHeight = childrenHeight + getPaddingTop() + getPaddingBottom();
        setMeasuredDimension(measuredWidth, measuredHeight);
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
     * Set a pin bookmark listener to be used by this view.
     *
     * @param listener A pin bookmark listener for this view.
     */
    public void setOnPinBookmarkListener(OnPinBookmarkListener listener) {
        mPinBookmarkListener = listener;
    }

    /**
     * A ContextMenuInfo for TopBoomarksView that adds details from the cursor.
     */
    public static class TopBookmarksContextMenuInfo extends AdapterContextMenuInfo {
        public String url;
        public String title;
        public boolean isPinned;

        public TopBookmarksContextMenuInfo(View targetView, int position, long id, Cursor cursor) {
            super(targetView, position, id);

            if (cursor == null) {
                return;
            }

            url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
            isPinned = ((TopSitesCursorWrapper) cursor).isPinned();
        }
    }
}
