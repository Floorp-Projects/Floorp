/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.BookmarksListAdapter.OnRefreshFolderListener;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.TopBookmarksView.Thumbnail;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A page in about:home that displays a ListView of bookmarks.
 */
public class BookmarksPage extends HomeFragment {
    public static final String LOGTAG = "GeckoBookmarksPage";

    // Cursor loader ID for list of bookmarks.
    private static final int BOOKMARKS_LIST_LOADER_ID = 0;

    // Cursor loader ID for grid of bookmarks.
    private static final int TOP_BOOKMARKS_LOADER_ID = 1;

    // Key for bookmarks folder id.
    private static final String BOOKMARKS_FOLDER_KEY = "folder_id";

    // List of bookmarks.
    private BookmarksListView mList;

    // Grid of top bookmarks.
    private TopBookmarksView mTopBookmarks;

    // Adapter for list of bookmarks.
    private BookmarksListAdapter mListAdapter;

    // Adapter for grid of bookmarks.
    private TopBookmarksAdapter mTopBookmarksAdapter;

    // Callback for loaders.
    private CursorLoaderCallbacks mLoaderCallbacks;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        BookmarksListView list = (BookmarksListView) inflater.inflate(R.layout.home_bookmarks_page, container, false);

        mTopBookmarks = new TopBookmarksView(getActivity());
        list.addHeaderView(mTopBookmarks);

        return list;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        OnUrlOpenListener listener = null;
        try {
            listener = (OnUrlOpenListener) getActivity();
        } catch (ClassCastException e) {
            throw new ClassCastException(getActivity().toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }

        mList = (BookmarksListView) view.findViewById(R.id.bookmarks_list);
        mList.setOnUrlOpenListener(listener);

        registerForContextMenu(mList);

        mTopBookmarks.setOnUrlOpenListener(listener);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Setup the top bookmarks adapter.
        mTopBookmarksAdapter = new TopBookmarksAdapter(getActivity(), null);
        mTopBookmarks.setAdapter(mTopBookmarksAdapter);

        // Setup the list adapter.
        mListAdapter = new BookmarksListAdapter(getActivity(), null);
        mListAdapter.setOnRefreshFolderListener(new OnRefreshFolderListener() {
            @Override
            public void onRefreshFolder(int folderId) {
                // Restart the loader with folder as the argument.
                Bundle bundle = new Bundle();
                bundle.putInt(BOOKMARKS_FOLDER_KEY, folderId);
                getLoaderManager().restartLoader(BOOKMARKS_LIST_LOADER_ID, bundle, mLoaderCallbacks);
            }
        });
        mList.setAdapter(mListAdapter);

        // Create callbacks before the initial loader is started.
        mLoaderCallbacks = new CursorLoaderCallbacks();

        // Reconnect to the loader only if present.
        final LoaderManager manager = getLoaderManager();
        manager.initLoader(BOOKMARKS_LIST_LOADER_ID, null, mLoaderCallbacks);
        manager.initLoader(TOP_BOOKMARKS_LOADER_ID, null, mLoaderCallbacks);
    }

    @Override
    public void onDestroyView() {
        mList = null;
        mListAdapter = null;
        mTopBookmarks = null;
        mTopBookmarksAdapter = null;
        super.onDestroyView();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reattach the fragment, forcing a reinflation of its view.
        // We use commitAllowingStateLoss() instead of commit() here to avoid
        // an IllegalStateException. If the phone is rotated while Fennec
        // is in the background, onConfigurationChanged() is fired.
        // onConfigurationChanged() is called before onResume(), so
        // using commit() would throw an IllegalStateException since it can't
        // be used between the Activity's onSaveInstanceState() and
        // onResume().
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    /**
     * Loader for the list for bookmarks.
     */
    private static class BookmarksLoader extends SimpleCursorLoader {
        private final int mFolderId;

        public BookmarksLoader(Context context) {
            this(context, Bookmarks.FIXED_ROOT_ID);
        }

        public BookmarksLoader(Context context, int folderId) {
            super(context);
            mFolderId = folderId;
        }

        @Override
        public Cursor loadCursor() {
            return BrowserDB.getBookmarksInFolder(getContext().getContentResolver(), mFolderId);
        }
    }

    /**
     * Loader for the grid for top bookmarks.
     */
    private static class TopBookmarksLoader extends SimpleCursorLoader {
        public TopBookmarksLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            final int max = getContext().getResources().getInteger(R.integer.number_of_top_sites);
            return BrowserDB.getTopSites(getContext().getContentResolver(), max);
        }
    }

    /**
     * Loader callbacks for the LoaderManager of this fragment.
     */
    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            switch(id) {
                case BOOKMARKS_LIST_LOADER_ID: {
                    if (args == null) {
                        return new BookmarksLoader(getActivity());
                    } else {
                        return new BookmarksLoader(getActivity(), args.getInt(BOOKMARKS_FOLDER_KEY));
                    }
                }

                case TOP_BOOKMARKS_LOADER_ID: {
                    return new TopBookmarksLoader(getActivity());
                }
            }

            return null;
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch(loaderId) {
                case BOOKMARKS_LIST_LOADER_ID: {
                    mListAdapter.swapCursor(c);
                    break;
                }

                case TOP_BOOKMARKS_LOADER_ID: {
                    mTopBookmarksAdapter.swapCursor(c);

                    // Load the thumbnails.
                    if (c.getCount() > 0) {
                        new LoadThumbnailsTask(getActivity(), mTopBookmarks).execute(c);
                    }
                    break;
                }
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final int loaderId = loader.getId();
            switch(loaderId) {
                case BOOKMARKS_LIST_LOADER_ID: {
                    if (mList != null) {
                        mListAdapter.swapCursor(null);
                    }
                    break;
                }

                case TOP_BOOKMARKS_LOADER_ID: {
                    if (mTopBookmarks != null) {
                        mTopBookmarksAdapter.swapCursor(null);
                        break;
                    }
                }
            }
        }
    }

    /**
     * An AsyncTask to load the thumbnails from a cursor.
     */
    private static class LoadThumbnailsTask extends UiAsyncTask<Cursor, Void, Map<String, Thumbnail>> {
        private final Context mContext;
        private final TopBookmarksView mView;

        public LoadThumbnailsTask(Context context, TopBookmarksView view) {
            super(ThreadUtils.getBackgroundHandler());
            mContext = context;
            mView = view;
        }

        @Override
        protected Map<String, Thumbnail> doInBackground(Cursor... params) {
            // TopBookmarksAdapter's cursor.
            final Cursor adapterCursor = params[0];
            if (adapterCursor == null || !adapterCursor.moveToFirst()) {
                return null;
            }

            final List<String> urls = new ArrayList<String>();
            do {
                final String url = adapterCursor.getString(adapterCursor.getColumnIndexOrThrow(URLColumns.URL));
                urls.add(url);
            } while (adapterCursor.moveToNext());

            if (urls.size() == 0) {
                return null;
            }

            final Map<String, Thumbnail> thumbnails = new HashMap<String, Thumbnail>();

            // Query the DB for thumbnails.
            final ContentResolver cr = mContext.getContentResolver();
            final Cursor cursor = BrowserDB.getThumbnailsForUrls(cr, urls);

            try {
                if (cursor != null && cursor.moveToFirst()) {
                    do {
                        // Try to get the thumbnail, if cursor is valid.
                        String url = cursor.getString(cursor.getColumnIndexOrThrow(Thumbnails.URL));
                        final byte[] b = cursor.getBlob(cursor.getColumnIndexOrThrow(Thumbnails.DATA));
                        final Bitmap bitmap = (b == null || b.length == 0 ? null : BitmapUtils.decodeByteArray(b));

                        if (bitmap != null) {
                            thumbnails.put(url, new Thumbnail(bitmap, true));
                        }
                    } while (cursor.moveToNext());
                }
            } finally {
                if (cursor != null) {
                    cursor.close();
                }
            }

            // Query the DB for favicons for the urls without thumbnails.
            for (String url : urls) {
                if (!thumbnails.containsKey(url)) {
                    final Bitmap bitmap = BrowserDB.getFaviconForUrl(cr, url);
                    if (bitmap != null) {
                        // Favicons.scaleImage can return several different size favicons,
                        // but will at least prevent this from being too large.
                        thumbnails.put(url, new Thumbnail(Favicons.getInstance().scaleImage(bitmap), false));
                    }
                }
            }

            return thumbnails;
        }

        @Override
        public void onPostExecute(Map<String, Thumbnail> thumbnails) {
            // Check to see if the view is still attached.
            if (mView.getHandler() != null) {
                mView.updateThumbnails(thumbnails);
            }
        }
    }
}
