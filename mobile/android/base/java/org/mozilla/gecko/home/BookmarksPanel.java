/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

import org.mozilla.gecko.EditBookmarkDialog;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.bookmarks.BookmarkEditFragment;
import org.mozilla.gecko.bookmarks.BookmarkUtils;
import org.mozilla.gecko.bookmarks.EditBookmarkTask;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.distribution.PartnerBookmarksProviderProxy;
import org.mozilla.gecko.home.BookmarksListAdapter.FolderInfo;
import org.mozilla.gecko.home.BookmarksListAdapter.OnRefreshFolderListener;
import org.mozilla.gecko.home.BookmarksListAdapter.RefreshType;
import org.mozilla.gecko.home.HomeContextMenuInfo.RemoveItemType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.preferences.GeckoPreferences;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.database.MergeCursor;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * A page in about:home that displays a ListView of bookmarks.
 */
public class BookmarksPanel extends HomeFragment implements BookmarkEditFragment.Callbacks {
    public static final String LOGTAG = "GeckoBookmarksPanel";

    // Cursor loader ID for list of bookmarks.
    private static final int LOADER_ID_BOOKMARKS_LIST = 0;

    // Information about the target bookmarks folder.
    private static final String BOOKMARKS_FOLDER_INFO = "folder_info";

    // Refresh type for folder refreshing loader.
    private static final String BOOKMARKS_REFRESH_TYPE = "refresh_type";

    // Position that the list view should be scrolled to after loading has finished.
    private static final String BOOKMARKS_SCROLL_POSITION = "listview_position";

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

    // Keep track whether a fresh loader has been used or not.
    private int mLastLoaderHash;

    @Override
    public void restoreData(@NonNull Bundle data) {
        final ArrayList<FolderInfo> stack = data.getParcelableArrayList("parentStack");
        if (stack == null) {
            return;
        }

        if (mListAdapter == null) {
            mSavedParentStack = new LinkedList<FolderInfo>(stack);
        } else {
            mListAdapter.restoreData(stack);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.home_bookmarks_panel, container, false);

        mList = (BookmarksListView) view.findViewById(R.id.bookmarks_list);

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
                final boolean enableFullBookmarkManagement = BookmarkUtils.isEnabled(getContext());
                if (!enableFullBookmarkManagement && type == Bookmarks.TYPE_FOLDER) {
                    // We don't show a context menu for folders if full bookmark management isn't enabled.
                    return null;
                }
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.TITLE));
                info.bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                info.itemType = RemoveItemType.BOOKMARKS;

                if (type == Bookmarks.TYPE_FOLDER) {
                    info.isFolder = true;
                    info.url = "";
                }
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
            public void onRefreshFolder(FolderInfo folderInfo,
                                        RefreshType refreshType ,
                                        int targetPosition) {
                // Restart the loader with folder as the argument.
                Bundle bundle = new Bundle();
                bundle.putParcelable(BOOKMARKS_FOLDER_INFO, folderInfo);
                bundle.putParcelable(BOOKMARKS_REFRESH_TYPE, refreshType);
                bundle.putInt(BOOKMARKS_SCROLL_POSITION, targetPosition);
                getLoaderManager().restartLoader(LOADER_ID_BOOKMARKS_LIST, bundle, mLoaderCallbacks);
            }
        });
        mList.setAdapter(mListAdapter);

        // Create callbacks before the initial loader is started.
        mLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (super.onContextItemSelected(item)) {
            // HomeFragment was able to handle to selected item.
            return true;
        }

        final ContextMenuInfo menuInfo = item.getMenuInfo();
        if (!(menuInfo instanceof HomeContextMenuInfo)) {
            return false;
        }

        final HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

        final int itemId = item.getItemId();
        final Context context = getContext();

        if (itemId == R.id.home_edit_bookmark) {
            if (BookmarkUtils.isEnabled(getContext())) {
                final BookmarkEditFragment dialog = BookmarkEditFragment.newInstance(info.bookmarkId);
                dialog.setTargetFragment(this, 0);
                dialog.show(getFragmentManager(), "edit-bookmark");
            } else {
                // UI Dialog associates to the activity context, not the applications'.
                new EditBookmarkDialog(context).show(info.url);
            }
            return true;
        }

        return false;
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

        if (isVisible()) {
            // The parent stack is saved just so that the folder state can be
            // restored on rotation.
            mSavedParentStack = mListAdapter.getParentStack();
        }
    }

    @Override
    protected void load() {
        final Bundle bundle;
        if (mSavedParentStack != null && mSavedParentStack.size() > 1) {
            bundle = new Bundle();
            bundle.putParcelable(BOOKMARKS_FOLDER_INFO, mSavedParentStack.get(0));
            bundle.putParcelable(BOOKMARKS_REFRESH_TYPE, RefreshType.CHILD);
        } else {
            bundle = null;
        }

        getLoaderManager().initLoader(LOADER_ID_BOOKMARKS_LIST, bundle, mLoaderCallbacks);
    }

    @Override
    public void onEditBookmark(@NonNull Bundle bundle) {
        new EditBookmarkTask(getActivity(), bundle, this).execute();
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
        private final int mTargetPosition;
        private final BrowserDB mDB;

        public BookmarksLoader(Context context) {
            this(context,
                 new FolderInfo(Bookmarks.FIXED_ROOT_ID, context.getResources().getString(R.string.bookmarks_title), 0),
                 RefreshType.CHILD, 0);
        }

        public BookmarksLoader(Context context,
                               FolderInfo folderInfo,
                               RefreshType refreshType,
                               int targetPosition) {
            super(context);
            mFolderInfo = folderInfo;
            mRefreshType = refreshType;
            mTargetPosition = targetPosition;
            mDB = BrowserDB.from(context);
        }

        @Override
        public Cursor loadCursor() {
            final boolean isRootFolder = mFolderInfo.id == BrowserContract.Bookmarks.FIXED_ROOT_ID;

            final ContentResolver contentResolver = getContext().getContentResolver();

            Cursor partnerCursor = null;
            Cursor userCursor = null;

            if (GeckoSharedPrefs.forProfile(getContext()).getBoolean(GeckoPreferences.PREFS_READ_PARTNER_BOOKMARKS_PROVIDER, false)
                    && (isRootFolder || mFolderInfo.id <= Bookmarks.FAKE_PARTNER_BOOKMARKS_START)) {
                partnerCursor = contentResolver.query(PartnerBookmarksProviderProxy.getUriForBookmarks(getContext(), mFolderInfo.id), null, null, null, null);
            }

            if (isRootFolder || mFolderInfo.id > Bookmarks.FAKE_PARTNER_BOOKMARKS_START) {
                userCursor = mDB.getBookmarksInFolder(contentResolver, mFolderInfo.id);
            }

            // MergeCursor is only partly capable of handling null cursors, hence the complicated
            // logic here. The main issue is CursorAdapter always queries the _id column when
            // swapping a cursor. If you haven't started iterating over the cursor, MergeCursor will
            // try to fetch columns from the first Cursor in the list - if that item is null,
            // we can't getColumnIndexOrThrow("_id"), and CursorAdapter crashes.
            if (partnerCursor == null && userCursor == null) {
                return null;
            } else if (partnerCursor == null) {
                return userCursor;
            } else if (userCursor == null) {
                return partnerCursor;
            } else {
                return new MergeCursor(new Cursor[]{ partnerCursor, userCursor });
            }


        }

        @Override
        public void onContentChanged() {
            // Invalidate the cached value that keeps track of whether or
            // not desktop bookmarks exist.
            mDB.invalidate();
            super.onContentChanged();
        }

        public FolderInfo getFolderInfo() {
            return mFolderInfo;
        }

        public RefreshType getRefreshType() {
            return mRefreshType;
        }

        public int getTargetPosition() {
            return mTargetPosition;
        }
    }

    /**
     * Loader callbacks for the LoaderManager of this fragment.
     */
    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (args == null) {
                return new BookmarksLoader(getActivity());
            } else {
                FolderInfo folderInfo = (FolderInfo) args.getParcelable(BOOKMARKS_FOLDER_INFO);
                RefreshType refreshType = (RefreshType) args.getParcelable(BOOKMARKS_REFRESH_TYPE);
                final int targetPosition = args.getInt(BOOKMARKS_SCROLL_POSITION);
                return new BookmarksLoader(getActivity(), folderInfo, refreshType, targetPosition);
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final BookmarksLoader bl = (BookmarksLoader) loader;
            mListAdapter.swapCursor(c, bl.getFolderInfo(), bl.getRefreshType());

            if (mPanelStateChangeListener != null) {
                final List<FolderInfo> parentStack = mListAdapter.getParentStack();
                final Bundle bundle = new Bundle();

                // Bundle likes to store ArrayLists or Arrays, but we've got a generic List (which
                // is actually an unmodifiable wrapper around a LinkedList). We'll need to do a
                // LinkedList conversion at the other end, when saving we need to use this awkward
                // syntax to create an Array.
                bundle.putParcelableArrayList("parentStack", new ArrayList<FolderInfo>(parentStack));

                mPanelStateChangeListener.onStateChanged(bundle);
            }

            // BrowserDB updates (e.g. through sync, or when opening a new tab) will trigger
            // a refresh which reuses the same loader - in that case we don't want to reset
            // the scroll position again.
            final int currentLoaderHash = bl.hashCode();
            if (mList != null && currentLoaderHash != mLastLoaderHash) {
                mList.setSelection(bl.getTargetPosition());
                mLastLoaderHash = currentLoaderHash;
            }
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
