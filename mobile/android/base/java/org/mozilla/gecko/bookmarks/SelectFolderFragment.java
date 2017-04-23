/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.bookmarks;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.widget.FadedSingleColorTextView;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A dialog fragment that allows selecting a bookmark folder.
 */
public class SelectFolderFragment extends DialogFragment {

    private static final String ARG_PARENT_ID = "parentId";
    private static final String ARG_BOOKMARK_ID = "bookmarkId";

    private static final int LOADER_ID_FOLDERS = 0;

    private long parentId;
    private long bookmarkId;

    private Toolbar toolbar;
    private RecyclerView folderView;
    private FolderListAdapter foldersAdapter;

    private SelectFolderCallback callback;

    public static SelectFolderFragment newInstance(long parentId, long bookmarkId) {
        final Bundle args = new Bundle();
        args.putLong(ARG_PARENT_ID, parentId);
        args.putLong(ARG_BOOKMARK_ID, bookmarkId);

        final SelectFolderFragment fragment = new SelectFolderFragment();
        fragment.setArguments(args);

        return fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        final Fragment fragment = getTargetFragment();
        if (fragment != null && fragment instanceof SelectFolderCallback) {
            callback = (SelectFolderCallback) fragment;
        }
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.Bookmark_Gecko);

        final Bundle args = getArguments();
        if (args != null) {
            parentId = args.getLong(ARG_PARENT_ID);
            bookmarkId = args.getLong(ARG_BOOKMARK_ID);
        }
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.bookmark_folder_select, container);
        toolbar = (Toolbar) view.findViewById(R.id.toolbar);
        folderView = (RecyclerView) view.findViewById(R.id.folder_recycler_view);

        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });

        foldersAdapter = new FolderListAdapter();
        folderView.setAdapter(foldersAdapter);

        folderView.setHasFixedSize(true);

        final LinearLayoutManager llm = new LinearLayoutManager(getContext());
        folderView.setLayoutManager(llm);

        return view;
    }

    @Override
    public void onResume() {
        super.onResume();

        getLoaderManager().initLoader(LOADER_ID_FOLDERS, null, new FoldersLoaderCallbacks());
    }

    @Override
    public void onPause() {
        super.onPause();

        getLoaderManager().destroyLoader(LOADER_ID_FOLDERS);
    }

    public void onFolderSelected(final Folder folder) {
        // Ignore fake folders.
        if (folder.id == BrowserContract.Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
            return;
        }

        final String title;
        if (BrowserContract.Bookmarks.MOBILE_FOLDER_GUID.equals(folder.guid)) {
            title = getString(R.string.bookmarks_folder_mobile);
        } else {
            title = folder.title;
        }

        if (callback != null) {
            callback.onFolderChanged(folder.id, title);
        }
        dismiss();
    }

    /**
     * A private struct to make it easier to pass bookmark data across threads
     */
    private static class Folder {
        private final long id;
        private final String title;
        private final String guid;

        int paddingLevel = 0;

        private Folder(long id, String title, String guid) {
            this.id = id;
            this.title = title;
            this.guid = guid;
        }
    }

    private class FoldersLoaderCallbacks implements LoaderManager.LoaderCallbacks<List<Folder>> {
        @Override
        public Loader<List<Folder>> onCreateLoader(int id, Bundle args) {
            return new FoldersLoader(getContext(), bookmarkId);
        }

        @Override
        public void onLoadFinished(Loader<List<Folder>> loader, final List<Folder> folders) {
            if (folders == null) {
                return;
            }
            foldersAdapter.addAll(folders);
        }

        @Override
        public void onLoaderReset(Loader<List<Folder>> loader) {
        }
    }

    /**
     * An AsyncTaskLoader to load {@link Folder} from a cursor.
     */
    private static class FoldersLoader extends AsyncTaskLoader<List<Folder>> {
        private final ContentResolver cr;
        private final BrowserDB db;
        private long bookmarkId;
        private List<Folder> folders;

        private FoldersLoader(Context context, long bookmarkId) {
            super(context);

            cr = context.getContentResolver();
            db = BrowserDB.from(context);
            this.bookmarkId = bookmarkId;
        }

        @Override
        public List<Folder> loadInBackground() {
            // The data order in cursor is 'ASC TYPE, ASC POSITION, ASC ID'.
            final Cursor cursor = db.getAllBookmarkFolders(cr);
            if (cursor == null) {
                return null;
            }

            final int capacity = cursor.getCount();

            @SuppressLint("UseSparseArrays") final Map<Long, List<Folder>> folderMap = new HashMap<>(capacity);
            try {
                while (cursor.moveToNext()) {
                    final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                    final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
                    final String guid = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID));

                    final long parentId;
                    switch (guid) {
                        case BrowserContract.Bookmarks.TOOLBAR_FOLDER_GUID:
                        case BrowserContract.Bookmarks.UNFILED_FOLDER_GUID:
                        case BrowserContract.Bookmarks.MENU_FOLDER_GUID:
                            parentId = BrowserContract.Bookmarks.FAKE_DESKTOP_FOLDER_ID;
                            break;

                        case BrowserContract.Bookmarks.MOBILE_FOLDER_GUID:
                        case BrowserContract.Bookmarks.FAKE_DESKTOP_FOLDER_GUID:
                            parentId = BrowserContract.Bookmarks.FIXED_ROOT_ID;
                            break;

                        default:
                            parentId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
                            break;
                    }

                    List<Folder> childFolders = folderMap.get(parentId);
                    if (childFolders == null) {
                        childFolders = new ArrayList<>();
                    }
                    childFolders.add(new Folder(id, title, guid));

                    folderMap.put(parentId, childFolders);
                }
            } finally {
                cursor.close();
            }

            folders = flattenChildFolders(folderMap, capacity);
            return folders;
        }

        private List<Folder> flattenChildFolders(Map<Long, List<Folder>> map, int capacity) {
            final List<Folder> result = new ArrayList<>(capacity);

            // Here we use ArrayDeque as a stack because it's considered faster than java.util.Stack.
            // https://developer.android.com/reference/java/util/ArrayDeque.html
            final ArrayDeque<Folder> folderQueue = new ArrayDeque<>(capacity);

            // Add "mobile bookmarks" and "desktop bookmarks" into arrayDeque.
            final List<Folder> first2Children = map.get((long) BrowserContract.Bookmarks.FIXED_ROOT_ID);
            for (Folder child : first2Children) {
                if (!BrowserContract.Bookmarks.MOBILE_FOLDER_GUID.equals(child.guid) &&
                    !BrowserContract.Bookmarks.FAKE_DESKTOP_FOLDER_GUID.equals(child.guid)) {
                    continue;
                }
                folderQueue.add(child);
            }

            while (!folderQueue.isEmpty()) {
                final Folder folder = folderQueue.pop();
                result.add(folder);

                final List<Folder> children = map.get(folder.id);
                if (children == null || children.size() == 0) {
                    continue;
                }
                for (int i = children.size() - 1; i >= 0; --i) {
                    final Folder child = children.get(i);

                    // Ignore itself and descendant folders because we don't allow to select itself or its descendants.
                    if (child.id == bookmarkId) {
                        continue;
                    }

                    child.paddingLevel = folder.paddingLevel + 1;
                    folderQueue.push(child);
                }
            }
            return result;
        }

        @Override
        public void deliverResult(List<Folder> folderList) {
            if (isReset()) {
                folders = null;
                return;
            }

            folders = folderList;

            if (isStarted()) {
                super.deliverResult(folderList);
            }
        }

        @Override
        protected void onStartLoading() {
            if (folders != null) {
                deliverResult(folders);
            }

            if (takeContentChanged() || folders == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(List<Folder> folderList) {
            folders = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            folders = null;
        }
    }

    private class FolderListAdapter extends RecyclerView.Adapter<FolderListAdapter.ViewHolder> {
        private final int firstStartPadding;
        private final int startPadding;
        private final List<Folder> folders;

        private FolderListAdapter() {
            final Resources res = getResources();
            firstStartPadding = (int) res.getDimension(R.dimen.bookmark_folder_first_child_padding);
            startPadding = (int) res.getDimension(R.dimen.bookmark_folder_child_padding);

            folders = new ArrayList<>();
        }

        private void addAll(List<Folder> folderList) {
            folders.clear();
            folders.addAll(folderList);

            notifyDataSetChanged();
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            final View itemView = LayoutInflater.from(parent.getContext())
                                          .inflate(R.layout.bookmark_folder_item, parent, false);
            return new ViewHolder(itemView);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            final Folder folder = folders.get(position);

            if (BrowserContract.Bookmarks.MOBILE_FOLDER_GUID.equals(folder.guid)) {
                holder.titleView.setText(R.string.bookmarks_folder_mobile);
            } else if (folder.id == BrowserContract.Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
                holder.titleView.setText(R.string.bookmarks_folder_desktop);
            } else {
                holder.titleView.setText(folder.title);
            }

            final int startPadding;
            if (folder.paddingLevel == 0) {
                startPadding = 0;
            } else {
                startPadding = firstStartPadding + (folder.paddingLevel - 1) * this.startPadding;
            }

            ViewCompat.setPaddingRelative(holder.container,
                                          startPadding,
                                          holder.container.getPaddingTop(),
                                          ViewCompat.getPaddingEnd(holder.container),
                                          holder.container.getPaddingBottom());

            final boolean isSelected = (folder.id == parentId);
            holder.selectView.setVisibility(isSelected ? View.VISIBLE : View.GONE);

            if (folder.paddingLevel == 0) {
                holder.divider.setVisibility(View.VISIBLE);
                holder.itemView.setBackgroundResource(R.color.about_page_header_grey);
            } else {
                holder.divider.setVisibility(View.GONE);
                holder.itemView.setBackgroundResource(R.color.bookmark_folder_bg_color);
            }
        }

        @Override
        public int getItemCount() {
            return folders.size();
        }

        class ViewHolder extends RecyclerView.ViewHolder {
            private final View divider;
            private final View container;
            private final FadedSingleColorTextView titleView;
            private final ImageView selectView;

            public ViewHolder(View itemView) {
                super(itemView);

                divider = itemView.findViewById(R.id.divider);
                container = itemView.findViewById(R.id.container);
                titleView = (FadedSingleColorTextView) itemView.findViewById(R.id.title);
                selectView = (ImageView) itemView.findViewById(R.id.select);

                itemView.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        final Folder folder = folders.get(getAdapterPosition());
                        onFolderSelected(folder);
                    }
                });
            }
        }
    }
}
