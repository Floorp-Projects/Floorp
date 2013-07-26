/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.BookmarksListAdapter.OnRefreshFolderListener;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PinBookmarkDialog.OnBookmarkSelectedListener;
import org.mozilla.gecko.home.TopBookmarksAdapter.Thumbnail;
import org.mozilla.gecko.home.TopBookmarksView.OnPinBookmarkListener;
import org.mozilla.gecko.home.TopBookmarksView.TopBookmarksContextMenuInfo;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.HashMap;
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

    // Loader ID for favicons.
    private static final int FAVICONS_LOADER_ID = 2;

    // Loader ID for thumbnails.
    private static final int THUMBNAILS_LOADER_ID = 3;

    // Key for bookmarks folder id.
    private static final String BOOKMARKS_FOLDER_KEY = "folder_id";

    // Key for thumbnail urls.
    private static final String THUMBNAILS_URLS_KEY = "urls";

    // List of bookmarks.
    private BookmarksListView mList;

    // Grid of top bookmarks.
    private TopBookmarksView mTopBookmarks;

    // Adapter for list of bookmarks.
    private BookmarksListAdapter mListAdapter;

    // Adapter for grid of bookmarks.
    private TopBookmarksAdapter mTopBookmarksAdapter;

    // Callback for cursor loaders.
    private CursorLoaderCallbacks mLoaderCallbacks;

    // Callback for thumbnail loader.
    private ThumbnailsLoaderCallbacks mThumbnailsLoaderCallbacks;

    // Listener for pinning bookmarks.
    private PinBookmarkListener mPinBookmarkListener;

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

        mPinBookmarkListener = new PinBookmarkListener();

        mList = (BookmarksListView) view.findViewById(R.id.bookmarks_list);
        mList.setOnUrlOpenListener(listener);

        mTopBookmarks.setOnUrlOpenListener(listener);
        mTopBookmarks.setOnPinBookmarkListener(mPinBookmarkListener);

        registerForContextMenu(mList);
        registerForContextMenu(mTopBookmarks);
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
        mThumbnailsLoaderCallbacks = new ThumbnailsLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void onDestroyView() {
        mList = null;
        mListAdapter = null;
        mTopBookmarks = null;
        mTopBookmarksAdapter = null;
        mPinBookmarkListener = null;
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

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (menuInfo == null) {
            return;
        }

        // HomeFragment will handle the default case.
        if (menuInfo instanceof HomeContextMenuInfo) {
            super.onCreateContextMenu(menu, view, menuInfo);
        }

        if (!(menuInfo instanceof TopBookmarksContextMenuInfo)) {
            return;
        }

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.top_bookmarks_contextmenu, menu);

        TopBookmarksContextMenuInfo info = (TopBookmarksContextMenuInfo) menuInfo;

        if (!TextUtils.isEmpty(info.url)) {
            if (info.isPinned) {
                menu.findItem(R.id.top_bookmarks_pin).setVisible(false);
            } else {
                menu.findItem(R.id.top_bookmarks_unpin).setVisible(false);
            }
        } else {
            menu.findItem(R.id.top_bookmarks_open_new_tab).setVisible(false);
            menu.findItem(R.id.top_bookmarks_open_private_tab).setVisible(false);
            menu.findItem(R.id.top_bookmarks_pin).setVisible(false);
            menu.findItem(R.id.top_bookmarks_unpin).setVisible(false);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        ContextMenuInfo menuInfo = item.getMenuInfo();

        // HomeFragment will handle the default case.
        if (menuInfo == null || !(menuInfo instanceof TopBookmarksContextMenuInfo)) {
            return false;
        }

        TopBookmarksContextMenuInfo info = (TopBookmarksContextMenuInfo) menuInfo;
        final Activity activity = getActivity();

        switch(item.getItemId()) {
            case R.id.top_bookmarks_open_new_tab:
            case R.id.top_bookmarks_open_private_tab: {
                if (info.url == null) {
                    Log.e(LOGTAG, "Can't open in new tab because URL is null");
                    break;
                }

                int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND;
                if (item.getItemId() == R.id.top_bookmarks_open_private_tab)
                    flags |= Tabs.LOADURL_PRIVATE;

                Tabs.getInstance().loadUrl(info.url, flags);
                Toast.makeText(activity, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
                return true;
            }

            case R.id.top_bookmarks_pin: {
                final String url = info.url;
                final String title = info.title;
                final int position = info.position;
                final Context context = getActivity().getApplicationContext();

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB.pinSite(context.getContentResolver(), url, title, position);
                    }
                });

                return true;
            }

            case R.id.top_bookmarks_unpin: {
                final int position = info.position;
                final Context context = getActivity().getApplicationContext();

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB.unpinSite(context.getContentResolver(), position);
                    }
                });

                return true;
            }

            case R.id.top_bookmarks_edit: {
                mPinBookmarkListener.onPinBookmark(info.position);
                return true;
            }
        }

        return false;
    }

    @Override
    protected void load() {
        final LoaderManager manager = getLoaderManager();
        manager.initLoader(BOOKMARKS_LIST_LOADER_ID, null, mLoaderCallbacks);
        manager.initLoader(TOP_BOOKMARKS_LOADER_ID, null, mLoaderCallbacks);
    }

    /**
     * Listener for pinning bookmarks.
     */
    private class PinBookmarkListener implements OnPinBookmarkListener,
                                                 OnBookmarkSelectedListener {
        // Tag for the PinBookmarkDialog fragment.
        private static final String TAG_PIN_BOOKMARK = "pin_bookmark";

        // Position of the pin.
        private int mPosition;

        @Override
        public void onPinBookmark(int position) {
            mPosition = position;

            final FragmentManager manager = getActivity().getSupportFragmentManager();
            PinBookmarkDialog dialog = (PinBookmarkDialog) manager.findFragmentByTag(TAG_PIN_BOOKMARK);
            if (dialog == null) {
                dialog = PinBookmarkDialog.newInstance();
            }

            dialog.setOnBookmarkSelectedListener(this);
            dialog.show(manager, TAG_PIN_BOOKMARK);
        }

        @Override
        public void onBookmarkSelected(final String url, final String title) {
            final int position = mPosition;
            final Context context = getActivity().getApplicationContext();
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    BrowserDB.pinSite(context.getContentResolver(), url, title, position);
                }
            });
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

                case FAVICONS_LOADER_ID: {
                    return FaviconsLoader.createInstance(getActivity(), args);
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
                    FaviconsLoader.restartFromCursor(getLoaderManager(), FAVICONS_LOADER_ID,
                            mLoaderCallbacks, c);
                    break;
                }

                case TOP_BOOKMARKS_LOADER_ID: {
                    mTopBookmarksAdapter.swapCursor(c);

                    // Load the thumbnails.
                    if (c.getCount() > 0 && c.moveToFirst()) {
                        final ArrayList<String> urls = new ArrayList<String>();
                        do {
                            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
                            urls.add(url);
                        } while (c.moveToNext());

                        if (urls.size() > 0) {
                            Bundle bundle = new Bundle();
                            bundle.putStringArrayList(THUMBNAILS_URLS_KEY, urls);
                            getLoaderManager().restartLoader(THUMBNAILS_LOADER_ID, bundle, mThumbnailsLoaderCallbacks);
                        }
                    }
                    break;
                }

                case FAVICONS_LOADER_ID: {
                    // Force the list to use in-memory favicons.
                    mListAdapter.notifyDataSetChanged();
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

                case FAVICONS_LOADER_ID: {
                    // Do nothing.
                    break;
                }
            }
        }
    }

    /**
     * An AsyncTaskLoader to load the thumbnails from a cursor.
     */
    private static class ThumbnailsLoader extends AsyncTaskLoader<Map<String, Thumbnail>> {
        private Map<String, Thumbnail> mThumbnails;
        private ArrayList<String> mUrls;

        public ThumbnailsLoader(Context context, ArrayList<String> urls) {
            super(context);
            mUrls = urls;
        }

        @Override
        public Map<String, Thumbnail> loadInBackground() {
            if (mUrls == null || mUrls.size() == 0) {
                return null;
            }

            final Map<String, Thumbnail> thumbnails = new HashMap<String, Thumbnail>();

            // Query the DB for thumbnails.
            final ContentResolver cr = getContext().getContentResolver();
            final Cursor cursor = BrowserDB.getThumbnailsForUrls(cr, mUrls);

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
            for (String url : mUrls) {
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
        public void deliverResult(Map<String, Thumbnail> thumbnails) {
            if (isReset()) {
                mThumbnails = null;
                return;
            }

            mThumbnails = thumbnails;

            if (isStarted()) {
                super.deliverResult(thumbnails);
            }
        }

        @Override
        protected void onStartLoading() {
            if (mThumbnails != null) {
                deliverResult(mThumbnails);
            }

            if (takeContentChanged() || mThumbnails == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(Map<String, Thumbnail> thumbnails) {
            mThumbnails = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            mThumbnails = null;
        }
    }

    /**
     * Loader callbacks for the thumbnails on TopBookmarksView.
     */
    private class ThumbnailsLoaderCallbacks implements LoaderCallbacks<Map<String, Thumbnail>> {
        @Override
        public Loader<Map<String, Thumbnail>> onCreateLoader(int id, Bundle args) {
            return new ThumbnailsLoader(getActivity(), args.getStringArrayList(THUMBNAILS_URLS_KEY));
        }

        @Override
        public void onLoadFinished(Loader<Map<String, Thumbnail>> loader, Map<String, Thumbnail> thumbnails) {
            if (mTopBookmarksAdapter != null) {
                mTopBookmarksAdapter.updateThumbnails(thumbnails);
            }
        }

        @Override
        public void onLoaderReset(Loader<Map<String, Thumbnail>> loader) {
            if (mTopBookmarksAdapter != null) {
                mTopBookmarksAdapter.updateThumbnails(null);
            }
        }
    }
}
