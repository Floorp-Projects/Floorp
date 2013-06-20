/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.TopSitesCursorWrapper;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.CursorAdapter;
import android.widget.GridView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A grid view of top bookmarks and pinned tabs.
 * Each cell in the grid is a TopBookmarkItemView.
 */
public class TopBookmarksView extends GridView {
    private static final String LOGTAG = "GeckoTopBookmarksView";

    // Max number of bookmarks that needs to be shown.
    private int mMaxBookmarks;

    // Number of columns to show.
    private int mNumColumns;

    // On URL open listener.
    private OnUrlOpenListener mUrlOpenListener;

    // A cursor based adapter backing this view.
    protected TopBookmarksAdapter mAdapter;

    // Temporary cache to store the thumbnails until the next layout pass.
    private Map<String, Bitmap> mThumbnailsCache;

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

        // Initialize the adapter.
        mAdapter = new TopBookmarksAdapter(getContext(), null);
        setAdapter(mAdapter);

        setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                TopBookmarkItemView row = (TopBookmarkItemView) view;
                String url = row.getUrl();

                if (mUrlOpenListener != null && !TextUtils.isEmpty(url)) {
                    mUrlOpenListener.onUrlOpen(url);
                }
            }
        });

        // Load the bookmarks.
        new LoadBookmarksTask().execute();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mAdapter != null) {
            setAdapter(null);
            final Cursor cursor = mAdapter.getCursor();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                if (cursor != null && !cursor.isClosed())
                    cursor.close();
                }
            });
        }
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

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        // If there are thumnails in the cache, update them.
        if (mThumbnailsCache != null) {
            updateThumbnails(mThumbnailsCache);
            mThumbnailsCache = null;
        }
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
     * Update the thumbnails returned by the db.
     *
     * @param thumbnails A map of urls and their thumbnail bitmaps.
     */
    private void updateThumbnails(Map<String, Bitmap> thumbnails) {
        final int count = mAdapter.getCount();
        for (int i = 0; i < count; i++) {
            final View child = getChildAt(i);

            // The grid view might get temporarily out of sync with the
            // adapter refreshes (e.g. on device rotation).
            if (child == null) {
                continue;
            }

            TopBookmarkItemView view = (TopBookmarkItemView) child;
            final String url = view.getUrl();

            // If there is no url, then show "add bookmark".
            if (TextUtils.isEmpty(url)) {
                view.displayThumbnail(R.drawable.abouthome_thumbnail_add);
            } else {
                // Show the thumbnail.
                Bitmap bitmap = (thumbnails != null ? thumbnails.get(url) : null);
                view.displayThumbnail(bitmap);
            }
        }
    }

    /**
     * A cursor adapter holding the pinned and top bookmarks.
     */
    public class TopBookmarksAdapter extends CursorAdapter {
        public TopBookmarksAdapter(Context context, Cursor cursor) {
            super(context, cursor);
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public int getCount() {
            return Math.min(super.getCount(), mMaxBookmarks);
        }

        /**
         * {@inheritDoc}
         */
        @Override
        protected void onContentChanged () {
            // Don't do anything. We don't want to regenerate every time
            // our database is updated.
            return;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void bindView(View bindView, Context context, Cursor cursor) {
            String url = "";
            String title = "";
            boolean pinned = false;

            // Cursor is already moved to required position.
            if (!cursor.isAfterLast()) {
                url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
                title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
                pinned = ((TopSitesCursorWrapper) cursor).isPinned();
            }

            TopBookmarkItemView view = (TopBookmarkItemView) bindView;
            view.setTitle(title);
            view.setUrl(url);
            view.setPinned(pinned);
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return new TopBookmarkItemView(context);
        }
    }

    /**
     * An AsyncTask to load the top bookmarks from the db.
     */
    private class LoadBookmarksTask extends UiAsyncTask<Void, Void, Cursor> {
        public LoadBookmarksTask() {
            super(ThreadUtils.getBackgroundHandler());
        }

        @Override
        protected Cursor doInBackground(Void... params) {
            return BrowserDB.getTopSites(getContext().getContentResolver(), mMaxBookmarks);
        }

        @Override
        public void onPostExecute(Cursor cursor) {
            // Change to new cursor.
            mAdapter.changeCursor(cursor);

            // Load the thumbnails.
            if (mAdapter.getCount() > 0) {
                new LoadThumbnailsTask().execute(cursor);
            }
        }
    }

    /**
     * An AsyncTask to load the thumbnails from a cursor.
     */
    private class LoadThumbnailsTask extends UiAsyncTask<Cursor, Void, Map<String,Bitmap>> {
        public LoadThumbnailsTask() {
            super(ThreadUtils.getBackgroundHandler());
        }

        @Override
        protected Map<String, Bitmap> doInBackground(Cursor... params) {
            // TopBookmarksAdapter's cursor.
            final Cursor adapterCursor = params[0];
            if (adapterCursor == null || !adapterCursor.moveToFirst()) {
                return null;
            }

            final List<String> urls = new ArrayList<String>();
            do {
                final String url = adapterCursor.getString(adapterCursor.getColumnIndexOrThrow(URLColumns.URL));
                urls.add(url);
            } while(adapterCursor.moveToNext());

            if (urls.size() == 0) {
                return null;
            }

            Map<String, Bitmap> thumbnails = new HashMap<String, Bitmap>();
            Cursor cursor = BrowserDB.getThumbnailsForUrls(getContext().getContentResolver(), urls);
            if (cursor == null || !cursor.moveToFirst()) {
                return null;
            }

            try {
                do {
                    final String url = cursor.getString(cursor.getColumnIndexOrThrow(Thumbnails.URL));
                    final byte[] b = cursor.getBlob(cursor.getColumnIndexOrThrow(Thumbnails.DATA));
                    if (b == null) {
                        continue;
                    }

                    Bitmap thumbnail = BitmapUtils.decodeByteArray(b);
                    if (thumbnail == null) {
                        continue;
                    }

                    thumbnails.put(url, thumbnail);
                } while (cursor.moveToNext());
            } finally {
                if (cursor != null) {
                    cursor.close();
                }
            }

            return thumbnails;
        }

        @Override
        public void onPostExecute(Map<String, Bitmap> thumbnails) {
            // If there's a layout scheduled on this view, wait for it to happen
            // by storing the thumbnails in a cache. If not, update them right away.
            if (isLayoutRequested()) {
                mThumbnailsCache = thumbnails;
            } else {
                updateThumbnails(thumbnails);
            }
        }
    }
}
