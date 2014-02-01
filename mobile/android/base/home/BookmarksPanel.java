/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.BookmarksListAdapter.FolderInfo;
import org.mozilla.gecko.home.BookmarksListAdapter.OnRefreshFolderListener;
import org.mozilla.gecko.home.BookmarksListAdapter.RefreshType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.List;

/**
 * A page in about:home that displays a ListView of bookmarks.
 */
public class BookmarksPanel extends HomeFragment {
    public static final String LOGTAG = "GeckoBookmarksPanel";

    // Cursor loader ID for list of bookmarks.
    private static final int LOADER_ID_BOOKMARKS_LIST = 0;

    // Information about the target bookmarks folder.
    private static final String BOOKMARKS_FOLDER_INFO = "folder_info";

    // Refresh type for folder refreshing loader.
    private static final String BOOKMARKS_REFRESH_TYPE = "refresh_type";

    // List of bookmarks.
    private BookmarksListView mList;

    // Adapter for list of bookmarks.
    private BookmarksListAdapter mListAdapter;

    // Adapter's parent stack.
    private List<FolderInfo> mSavedParentStack;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callback for cursor loaders.
    private CursorLoaderCallbacks mLoaderCallbacks;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.home_bookmarks_panel, container, false);

        mList = (BookmarksListView) view.findViewById(R.id.bookmarks_list);

        mList.setContextMenuInfoFactory(new HomeListView.ContextMenuInfoFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
                if (type == Bookmarks.TYPE_FOLDER) {
                    // We don't show a context menu for folders
                    return null;
                }
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.TITLE));
                info.bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                return info;
            }
        });

        return view;
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

        mList.setTag(HomePager.LIST_TAG_BOOKMARKS);
        mList.setOnUrlOpenListener(listener);

        registerForContextMenu(mList);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final Activity activity = getActivity();

        // Setup the list adapter.
        mListAdapter = new BookmarksListAdapter(activity, null, mSavedParentStack);
        mListAdapter.setOnRefreshFolderListener(new OnRefreshFolderListener() {
            @Override
            public void onRefreshFolder(FolderInfo folderInfo, RefreshType refreshType) {
                // Restart the loader with folder as the argument.
                Bundle bundle = new Bundle();
                bundle.putParcelable(BOOKMARKS_FOLDER_INFO, folderInfo);
                bundle.putParcelable(BOOKMARKS_REFRESH_TYPE, refreshType);
                getLoaderManager().restartLoader(LOADER_ID_BOOKMARKS_LIST, bundle, mLoaderCallbacks);
            }
        });
        mList.setAdapter(mListAdapter);

        // Invalidate the cached value that keeps track of whether or
        // not desktop bookmarks (or reading list items) exist.
        BrowserDB.invalidateCachedState();

        // Create callbacks before the initial loader is started.
        mLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void onDestroyView() {
        mList = null;
        mListAdapter = null;
        mEmptyView = null;
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
            // The parent stack is saved just so that the folder state can be
            // restored on rotation.
            mSavedParentStack = mListAdapter.getParentStack();

            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_BOOKMARKS_LIST, null, mLoaderCallbacks);
    }

    private void updateUiFromCursor(Cursor c) {
        if ((c == null || c.getCount() == 0) && mEmptyView == null) {
            // Set empty page view. We delay this so that the empty view won't flash.
            final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_bookmarks_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_bookmarks_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    /**
     * Loader for the list for bookmarks.
     */
    private static class BookmarksLoader extends SimpleCursorLoader {
        private final FolderInfo mFolderInfo;
        private final RefreshType mRefreshType;

        public BookmarksLoader(Context context) {
            this(context, new FolderInfo(Bookmarks.FIXED_ROOT_ID), RefreshType.CHILD);
        }

        public BookmarksLoader(Context context, FolderInfo folderInfo, RefreshType refreshType) {
            super(context);
            mFolderInfo = folderInfo;
            mRefreshType = refreshType;
        }

        @Override
        public Cursor loadCursor() {
            return BrowserDB.getBookmarksInFolder(getContext().getContentResolver(), mFolderInfo.id);
        }

        public FolderInfo getFolderInfo() {
            return mFolderInfo;
        }

        public RefreshType getRefreshType() {
            return mRefreshType;
        }
    }

    /**
     * Loader callbacks for the LoaderManager of this fragment.
     */
    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (args == null) {
                return new BookmarksLoader(getActivity());
            } else {
                FolderInfo folderInfo = (FolderInfo) args.getParcelable(BOOKMARKS_FOLDER_INFO);
                RefreshType refreshType = (RefreshType) args.getParcelable(BOOKMARKS_REFRESH_TYPE);
                return new BookmarksLoader(getActivity(), folderInfo, refreshType);
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            BookmarksLoader bl = (BookmarksLoader) loader;
            mListAdapter.swapCursor(c, bl.getFolderInfo(), bl.getRefreshType());
            updateUiFromCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (mList != null) {
                mListAdapter.swapCursor(null);
            }
        }
    }
}
