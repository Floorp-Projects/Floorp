/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.bookmarks;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.Nullable;
import android.support.design.widget.TextInputLayout;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.support.v7.widget.Toolbar;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;

import java.lang.ref.WeakReference;

/**
 * A dialog fragment that allows editing bookmark's url, title and changing the parent."
 */
public class BookmarkEditFragment extends DialogFragment implements SelectFolderCallback {

    private static final String ARG_ID = "id";
    private static final String ARG_URL = "url";
    private static final String ARG_BOOKMARK = "bookmark";

    private long bookmarkId;
    private String url;
    private Bookmark bookmark;

    private Toolbar toolbar;
    private EditText nameText;
    private TextInputLayout locationLayout;
    private EditText locationText;
    private TextInputLayout keywordLayout;
    private EditText keywordText;
    private EditText folderText;

    public interface Callbacks {
        /**
         * A callback method to tell caller that bookmark has been modified.
         * Caller takes charge for the change(e.g. update database).
         */
        void onEditBookmark(Bundle bundle);
    }
    private Callbacks callbacks;

    public static BookmarkEditFragment newInstance(long id) {
        final Bundle args = new Bundle();
        args.putLong(ARG_ID, id);

        return newInstance(args);
    }

    public static BookmarkEditFragment newInstance(String url) {
        final Bundle args = new Bundle();
        args.putString(ARG_URL, url);

        return newInstance(args);
    }

    private static BookmarkEditFragment newInstance(Bundle args) {
        final BookmarkEditFragment fragment = new BookmarkEditFragment();
        fragment.setArguments(args);

        return fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        Fragment fragment = getTargetFragment();
        if (fragment != null && fragment instanceof Callbacks) {
            callbacks = (Callbacks) fragment;
        } else if (context instanceof Callbacks) {
            callbacks = (Callbacks) context;
        }
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Apply DialogWhenLarge theme
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.Bookmark_Gecko);

        Bundle args = getArguments();
        bookmarkId = args.getLong(ARG_ID);
        url = args.getString(ARG_URL);
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.bookmark_edit_with_full_page, container);
        toolbar = (Toolbar) view.findViewById(R.id.toolbar);
        nameText = (EditText) view.findViewById(R.id.edit_bookmark_name);
        locationLayout = (TextInputLayout) view.findViewById(R.id.edit_bookmark_location_layout);
        locationText = (EditText) view.findViewById(R.id.edit_bookmark_location);
        keywordLayout = (TextInputLayout) view.findViewById(R.id.edit_bookmark_keyword_layout);
        keywordText = (EditText) view.findViewById(R.id.edit_bookmark_keyword);
        folderText = (EditText) view.findViewById(R.id.edit_parent_folder);

        toolbar.inflateMenu(R.menu.bookmark_edit_menu);
        toolbar.setOnMenuItemClickListener(new Toolbar.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                switch (item.getItemId()) {
                    case R.id.done:
                        final String newUrl = locationText.getText().toString().trim();
                        final String newTitle = nameText.getText().toString();
                        final String newKeyword = keywordText.getText().toString().trim();
                        if (callbacks != null) {
                            if (TextUtils.equals(newTitle, bookmark.originalTitle) &&
                                TextUtils.equals(newUrl, bookmark.originalUrl) &&
                                TextUtils.equals(newKeyword, bookmark.originalKeyword) &&
                                bookmark.parentId == bookmark.originalParentId) {
                                // Nothing changed, skip callback.
                                break;
                            }

                            final Bundle bundle = new Bundle();
                            bundle.putLong(Bookmarks._ID, bookmark.id);
                            bundle.putString(Bookmarks.TITLE, newTitle);
                            bundle.putString(Bookmarks.URL, newUrl);
                            bundle.putString(Bookmarks.KEYWORD, newKeyword);
                            bundle.putString(Bookmarks.OLD_TITLE, bookmark.originalTitle);
                            bundle.putString(Bookmarks.OLD_URL, bookmark.originalUrl);
                            bundle.putString(Bookmarks.OLD_KEYWORD, bookmark.originalKeyword);
                            if (bookmark.parentId != bookmark.originalParentId) {
                                bundle.putLong(Bookmarks.PARENT, bookmark.parentId);
                                bundle.putLong(BrowserContract.PARAM_OLD_BOOKMARK_PARENT, bookmark.originalParentId);
                            }
                            bundle.putInt(Bookmarks.TYPE, bookmark.type);

                            callbacks.onEditBookmark(bundle);
                        }
                        break;
                }

                dismiss();
                return true;
            }
        });
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });

        folderText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (bookmark == null) {
                    return;
                }

                // When coming back from SelectFolderFragment, we update the view with data stored in
                // `bookmark`, so before navigating, we have to save the current title and keyword
                // from the TextInputLayouts into `bookmark`.
                bookmark.title = nameText.getText().toString();
                bookmark.keyword = keywordText.getText().toString();

                final SelectFolderFragment dialog = SelectFolderFragment.newInstance(bookmark.parentId, bookmark.id);
                dialog.setTargetFragment(BookmarkEditFragment.this, 0);
                dialog.show(getActivity().getSupportFragmentManager(), "select-bookmark-folder");
            }
        });

        return view;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            final Bookmark bookmark = savedInstanceState.getParcelable(ARG_BOOKMARK);
            if (bookmark != null) {
                invalidateView(bookmark);
                return;
            }
        }

        getLoaderManager().initLoader(0, null, new BookmarkLoaderCallbacks());
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        getLoaderManager().destroyLoader(0);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (bookmark != null) {
            bookmark.url = locationText.getText().toString().trim();
            bookmark.title = nameText.getText().toString();
            bookmark.keyword = keywordText.getText().toString();
            bookmark.folder = folderText.getText().toString();
            outState.putParcelable(ARG_BOOKMARK, bookmark);
        }

        super.onSaveInstanceState(outState);
    }

    @Override
    public void onFolderChanged(long parentId, String title) {
        if (bookmark == null) {
            // Don't update view if bookmark isn't initialized yet.
            return;
        }

        bookmark.parentId = parentId;
        bookmark.folder = title;
        invalidateView(bookmark);
    }

    private void invalidateView(Bookmark bookmark) {
        this.bookmark = bookmark;

        nameText.setText(bookmark.title);

        if (bookmark.type == Bookmarks.TYPE_FOLDER) {
            toolbar.setTitle(R.string.bookmark_edit_folder_title);
            locationLayout.setVisibility(View.GONE);
            keywordLayout.setVisibility(View.GONE);
        } else {
            toolbar.setTitle(R.string.bookmark_edit_title);
            locationLayout.setVisibility(View.VISIBLE);
            keywordLayout.setVisibility(View.VISIBLE);
        }
        locationText.setText(bookmark.url);
        keywordText.setText(bookmark.keyword);

        if (Bookmarks.MOBILE_FOLDER_GUID.equals(bookmark.guid)) {
            folderText.setText(R.string.bookmarks_folder_mobile);
        } else {
            folderText.setText(bookmark.folder);
        }

        // Enable menu item after bookmark is set to view
        final MenuItem doneItem = toolbar.getMenu().findItem(R.id.done);
        doneItem.setEnabled(true);

        // Add a TextWatcher to prevent invalid input (e.g. an empty string).
        LocationTextWatcher locationTextWatcher = new LocationTextWatcher(doneItem);
        KeywordTextWatcher keywordTextWatcher = new KeywordTextWatcher(doneItem);

        // Cross reference the TextWatchers
        locationTextWatcher.setPairedTextWatcher(keywordTextWatcher);
        keywordTextWatcher.setPairedTextWatcher(locationTextWatcher);

        if (bookmark.type == Bookmarks.TYPE_FOLDER) {
            nameText.addTextChangedListener(locationTextWatcher);
        } else {
            locationText.addTextChangedListener(locationTextWatcher);
        }
        keywordText.addTextChangedListener(keywordTextWatcher);
    }

    /**
     * A private struct to make it easier to pass bookmark data across threads
     */
    private static class Bookmark implements Parcelable {
        // Cannot be modified in this fragment.
        final long id;
        final int type; // folder or bookmark
        final String guid;
        final String originalTitle;
        final String originalUrl;
        final String originalKeyword;
        final long originalParentId;
        final String originalFolder;

        // Can be modified in this fragment.
        String title;
        String url;
        String keyword;
        long parentId;
        String folder;

        public Bookmark(long id, String url, String title, String keyword, long parentId,
                        String folder, int type, String guid) {
            this(id, url, title, keyword, parentId, folder, type, guid, url, title, keyword, parentId, folder);
        }

        private Bookmark(long id, String originalUrl, String originalTitle, String originalKeyword,
                         long originalParentId, String originalFolder, int type, String guid,
                         String modifiedUrl, String modifiedTitle, String modifiedKeyword,
                         long modifiedParentId, String modifiedFolder) {
            this.id = id;
            this.originalUrl = originalUrl;
            this.originalTitle = originalTitle;
            this.originalKeyword = originalKeyword;
            this.originalParentId = originalParentId;
            this.originalFolder = originalFolder;
            this.type = type;
            this.guid = guid;

            this.url = modifiedUrl;
            this.title = modifiedTitle;
            this.keyword = modifiedKeyword;
            this.parentId = modifiedParentId;
            this.folder = modifiedFolder;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel parcel, int flags) {
            parcel.writeLong(id);
            parcel.writeString(url);
            parcel.writeString(title);
            parcel.writeString(keyword);
            parcel.writeLong(parentId);
            parcel.writeString(folder);
            parcel.writeInt(type);
            parcel.writeString(guid);
            parcel.writeString(originalUrl);
            parcel.writeString(originalTitle);
            parcel.writeString(originalKeyword);
            parcel.writeLong(originalParentId);
            parcel.writeString(originalFolder);
        }

        public static final Creator<Bookmark> CREATOR = new Creator<Bookmark>() {
            @Override
            public Bookmark createFromParcel(final Parcel source) {
                final long id = source.readLong();
                final String modifiedUrl = source.readString();
                final String modifiedTitle = source.readString();
                final String modifiedKeyword = source.readString();
                final long modifiedParentId = source.readLong();
                final String modifiedFolder = source.readString();
                final int type = source.readInt();
                final String guid = source.readString();

                final String originalUrl = source.readString();
                final String originalTitle = source.readString();
                final String originalKeyword = source.readString();
                final long originalParentId = source.readLong();
                final String originalFolder = source.readString();

                return new Bookmark(id, originalUrl, originalTitle, originalKeyword, originalParentId, originalFolder,
                                    type, guid, modifiedUrl, modifiedTitle, modifiedKeyword, modifiedParentId, modifiedFolder);
            }

            @Override
            public Bookmark[] newArray(final int size) {
                return new Bookmark[size];
            }
        };
    }

    private class BookmarkLoaderCallbacks implements LoaderManager.LoaderCallbacks<Bookmark> {
        @Override
        public Loader<Bookmark> onCreateLoader(int id, Bundle args) {
            return new BookmarkLoader(getContext(), bookmarkId, url);
        }

        @Override
        public void onLoadFinished(Loader<Bookmark> loader, final Bookmark bookmark) {
            if (bookmark == null) {
                return;
            }

            invalidateView(bookmark);
        }

        @Override
        public void onLoaderReset(Loader<Bookmark> loader) {
        }
    }

    /**
     * An AsyncTaskLoader to load {@link Bookmark} from a cursor.
     */
    private static class BookmarkLoader extends AsyncTaskLoader<Bookmark> {
        private final long bookmarkId;
        private final String url;
        private final ContentResolver contentResolver;
        private final BrowserDB db;
        private Bookmark bookmark;

        private BookmarkLoader(Context context, long id, String url) {
            super(context);

            this.bookmarkId = id;
            this.url = url;
            this.contentResolver = context.getContentResolver();
            this.db = BrowserDB.from(context);
        }

        @Override
        public Bookmark loadInBackground() {
            final Cursor cursor;

            if (url != null) {
                cursor = db.getBookmarkForUrl(contentResolver, url);
            } else {
                cursor = db.getBookmarkById(contentResolver, bookmarkId);
            }
            if (cursor == null) {
                return null;
            }

            Bookmark bookmark = null;
            try {
                if (cursor.moveToFirst()) {
                    final long id = cursor.getLong(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                    final String url = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL));
                    final String title = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.TITLE));
                    final String keyword = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.KEYWORD));

                    final long parentId = cursor.getLong(cursor.getColumnIndexOrThrow(Bookmarks.PARENT));
                    final String parentName = queryParentName(parentId);

                    final int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
                    final String guid = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.GUID));
                    bookmark = new Bookmark(id, url, title, keyword, parentId, parentName, type, guid);
                }
            } finally {
                cursor.close();
            }
            return bookmark;
        }

        private String queryParentName(long folderId) {
            Cursor cursor = db.getBookmarkById(contentResolver, folderId);
            if (cursor == null) {
                return "";
            }

            String folderName = "";
            try {
                if (cursor.moveToFirst()) {
                    final String guid = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.GUID));
                    if (Bookmarks.MOBILE_FOLDER_GUID.equals(guid)) {
                        folderName = getContext().getString(R.string.bookmarks_folder_mobile);
                    } else {
                        folderName = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.TITLE));
                    }
                }
            } finally {
                cursor.close();
            }
            return folderName;
        }

        @Override
        public void deliverResult(Bookmark bookmark) {
            if (isReset()) {
                this.bookmark = null;
                return;
            }

            this.bookmark = bookmark;

            if (isStarted()) {
                super.deliverResult(bookmark);
            }
        }

        @Override
        protected void onStartLoading() {
            if (bookmark != null) {
                deliverResult(bookmark);
            }

            if (takeContentChanged() || bookmark == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        public void onCanceled(Bookmark bookmark) {
            this.bookmark = null;
        }

        @Override
        protected void onReset() {
            super.onReset();

            // Ensure the loader is stopped.
            onStopLoading();

            bookmark = null;
        }
    }

    /**
     * This text watcher watches to enable or disable the Save button if the dialog contains
     * valid information. This class is overridden to do data checking on different fields.
     * By itself, it always enables the button.
     *
     * Callers can also assign a paired partner to the TextWatcher, and callers will check
     * that both are enabled before enabling the Save button.
     */
    private static class EditBookmarkTextWatcher implements TextWatcher {
        // A stored reference to the menu that should be disabled/enabled in response to the text
        // being watched.
        private final WeakReference<MenuItem> doneItemWeakReference;

        private EditBookmarkTextWatcher pairedTextWatcher;

        // Whether or not the menu item should be enabled.
        protected boolean enabled = true;

        private EditBookmarkTextWatcher(MenuItem doneItem) {
            doneItemWeakReference = new WeakReference<>(doneItem);
        }

        public void setPairedTextWatcher(EditBookmarkTextWatcher textWatcher) {
            pairedTextWatcher = textWatcher;
        }

        public boolean isEnabled() {
            return enabled;
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Disable if we're disabled or the paired partner is disabled.
            boolean enabled = this.enabled && (pairedTextWatcher == null || pairedTextWatcher.isEnabled());

            final MenuItem doneItem = doneItemWeakReference.get();
            if (doneItem != null) {
                doneItem.setEnabled(enabled);
            }
        }

        @Override
        public void afterTextChanged(Editable s) {}
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
    }

    /**
     * A version of the EditBookmarkTextWatcher for the URL field of the dialog.
     * Only checks if the field is empty or not.
     */
    private static final class LocationTextWatcher extends EditBookmarkTextWatcher {
        private LocationTextWatcher(MenuItem doneItem) {
            super(doneItem);
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Disables the menu item if the input field is empty.
            enabled = (s.toString().trim().length() > 0);
            super.onTextChanged(s, start, before, count);
        }
    }

    /**
     * A version of the EditBookmarkTextWatcher for the keyword field of the dialog.
     * Checks if the field has any (non leading or trailing) spaces.
     */
    private static final class KeywordTextWatcher extends EditBookmarkTextWatcher {
        private KeywordTextWatcher(MenuItem doneItem) {
            super(doneItem);
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Disable if the keyword contains spaces.
            enabled = (s.toString().trim().indexOf(' ') == -1);
            super.onTextChanged(s, start, before, count);
        }
    }
}
