/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.bookmarks;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

public class CreateFolderFragment extends DialogFragment implements SelectFolderCallback {

    private static final String ARG_PARENT_ID = "parentId";
    private static final String ARG_PARENT_TITLE = "parentTitle";
    private static final String ARG_TITLE = "title";

    private static final int LOADER_ID_FOLDER = 0;

    private Toolbar toolbar;
    private EditText nameText;
    private EditText folderText;

    private long parentId;

    private CreateFolderCallback callback;

    public static CreateFolderFragment newInstance() {
        return new CreateFolderFragment();
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        final Fragment fragment = getTargetFragment();
        if (fragment != null && fragment instanceof CreateFolderCallback) {
            callback = (CreateFolderCallback) fragment;
        }
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.Bookmark_Gecko);
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.bookmark_add_folder, container);
        toolbar = (Toolbar) view.findViewById(R.id.toolbar);
        nameText = (EditText) view.findViewById(R.id.edit_folder_name);
        folderText = (EditText) view.findViewById(R.id.edit_parent_folder);

        folderText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                SelectFolderFragment dialog = SelectFolderFragment.newInstance(parentId);
                dialog.setTargetFragment(CreateFolderFragment.this, 0);
                dialog.show(getActivity().getSupportFragmentManager(), "select-bookmark-folder");
            }
        });

        toolbar.inflateMenu(R.menu.bookmark_edit_menu);
        toolbar.setOnMenuItemClickListener(new Toolbar.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                switch (item.getItemId()) {
                    case R.id.done:
                        final String title = nameText.getText().toString();
                        final String defaultTitle = getString(R.string.bookmark_default_folder_title);
                        new AddFolderTask(parentId, TextUtils.isEmpty(title) ? defaultTitle : title).execute();
                }
                return true;
            }
        });
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });
        toolbar.getMenu().findItem(R.id.done).setEnabled(true);

        return view;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            parentId = savedInstanceState.getLong(ARG_PARENT_ID);

            final String parentTitle = savedInstanceState.getString(ARG_PARENT_TITLE);
            folderText.setText(parentTitle);

            final String title = savedInstanceState.getString(ARG_TITLE);
            nameText.setText(title);
            return;
        }
        getLoaderManager().initLoader(LOADER_ID_FOLDER, null, new FolderLoaderCallbacks());
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        getLoaderManager().destroyLoader(LOADER_ID_FOLDER);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putLong(ARG_PARENT_ID, parentId);

        final String parentTitle = folderText.getText().toString();
        outState.putString(ARG_PARENT_TITLE, parentTitle);

        final String title = nameText.getText().toString();
        outState.putString(ARG_TITLE, title);

        super.onSaveInstanceState(outState);
    }

    @Override
    public void onFolderChanged(long parentId, String title) {
        this.parentId = parentId;

        folderText.setText(title);
    }

    private class FolderLoaderCallbacks implements LoaderManager.LoaderCallbacks<Bundle> {

        @Override
        public Loader<Bundle> onCreateLoader(int id, Bundle args) {
            return new FolderLoader(getContext());
        }

        @Override
        public void onLoadFinished(Loader<Bundle> loader, final Bundle bundle) {
            if (bundle.isEmpty()) {
                return;
            }

            final long id = bundle.getLong(FolderLoader.ARG_ID);
            final String title = bundle.getString(ARG_TITLE);
            final String guid = bundle.getString(FolderLoader.ARG_GUID);

            parentId = id;

            if (BrowserContract.Bookmarks.MOBILE_FOLDER_GUID.equals(guid)) {
                folderText.setText(R.string.bookmarks_folder_mobile);
            } else {
                folderText.setText(title);
            }
        }

        @Override
        public void onLoaderReset(Loader<Bundle> loader) {
        }
    }

    private static class FolderLoader extends AsyncTaskLoader<Bundle> {
        private static final String ARG_ID = "id";
        private static final String ARG_GUID = "guid";

        private Bundle bundle;

        private FolderLoader(Context context) {
            super(context);
        }

        @Override
        public Bundle loadInBackground() {
            final Context context = getContext();
            final ContentResolver cr = context.getContentResolver();
            final BrowserDB db = BrowserDB.from(context);

            final Bundle bundle = new Bundle();
            final Cursor cursor = db.getBookmarkByGuid(cr, BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
            if (cursor == null) {
                throw new IllegalStateException("Cannot find folder 'Mobile Bookmarks'.");
            }

            try {
                if (!cursor.moveToFirst()) {
                    throw new IllegalStateException("Cannot find folder 'Mobile Bookmarks'.");
                }
                final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));

                bundle.putLong(ARG_ID, id);
                bundle.putString(ARG_TITLE, title);
                bundle.putString(ARG_GUID, BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
            } finally {
                cursor.close();
            }
            return bundle;
        }

        @Override
        public void deliverResult(Bundle bundle) {
            if (isReset()) {
                this.bundle = null;
                return;
            }

            this.bundle = bundle;

            if (isStarted()) {
                super.deliverResult(bundle);
            }
        }

        @Override
        protected void onStartLoading() {
            if (bundle != null) {
                deliverResult(bundle);
            }

            if (takeContentChanged() || bundle == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(Bundle bundle) {
            this.bundle = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            bundle = null;
        }
    }

    private class AddFolderTask extends UIAsyncTask.WithoutParams<Long> {
        private String title;
        private long parentId;

        private AddFolderTask(final long parentId, String title) {
            super(ThreadUtils.getBackgroundHandler());

            this.parentId = parentId;
            this.title = title;
        }

        @Override
        public Long doInBackground() {
            final Context context = getContext();
            final BrowserDB db = BrowserDB.from(context);
            final ContentResolver cr = context.getContentResolver();

            Telemetry.sendUIEvent(TelemetryContract.Event.SAVE,
                                  TelemetryContract.Method.DIALOG,
                                  "bookmark_folder");
            final Uri uri = db.addBookmarkFolder(cr, title, parentId);
            return Long.valueOf(uri.getLastPathSegment());
        }

        @Override
        public void onPostExecute(Long folderId) {
            if (callback != null) {
                callback.onFolderCreated(folderId, title);
            }

            dismiss();
        }
    }
}
