/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.Context;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.database.Cursor;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

/**
 * A dialog that allows editing a bookmarks url, title, or keywords
 * <p>
 * Invoked by calling one of the {@link org.mozilla.gecko.EditBookmarkDialog.show}
 * methods.
 */
public class EditBookmarkDialog {
    private static final String LOGTAG = "GeckoEditBookmarkDialog";
    private Context mContext;

    public EditBookmarkDialog(Context context) {
        mContext = context;
    }

    /**
     * A private struct to make it easier to pass bookmark data across threads
     */
    private class Bookmark {
        int id;
        String title;
        String url;
        String keyword;

        public Bookmark(int aId, String aTitle, String aUrl, String aKeyword) {
            id = aId;
            title = aTitle;
            url = aUrl;
            keyword = aKeyword;
        }
    }

    /**
     * This text watcher to enable or disable the OK button if the dialog contains
     * valid information. This class is overridden to do data checking diffferent fields.
     * By itself, it always enables the button.
     *
     * Callers can also assing a paired partner to the TextWatcher, and callers will check
     * that both are enabled before enabling the ok button.
     */
    private class EditBookmarkTextWatcher implements TextWatcher {
        // A stored reference to the dialog containing the text field being watched
        protected AlertDialog mDialog;

        // A stored text watcher to do the real verification of a field
        protected EditBookmarkTextWatcher mPairedTextWatcher;

        // Whether or not the ok button should be enabled.
        protected boolean mEnabled = true;

        public EditBookmarkTextWatcher(AlertDialog aDialog) {
            mDialog = aDialog;
        }

        public void setPairedTextWatcher(EditBookmarkTextWatcher aTextWatcher) {
            mPairedTextWatcher = aTextWatcher;
        }

        public boolean isEnabled() {
            return mEnabled;
        }

        // Textwatcher interface
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Disable if the we're disabled or the paired partner is disabled
            boolean enabled = mEnabled && (mPairedTextWatcher == null || mPairedTextWatcher.isEnabled());
            mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(enabled);
        }

        @Override
        public void afterTextChanged(Editable s) {}
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
    }

    /**
     * A version of the EditBookmarkTextWatcher for the url field of the dialog.
     * Only checks if the field is empty or not.
     */
    private class LocationTextWatcher extends EditBookmarkTextWatcher {
        public LocationTextWatcher(AlertDialog aDialog) {
            super(aDialog);
        }

        // Disables the ok button if the location field is empty.
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            mEnabled = (s.toString().trim().length() > 0);
            super.onTextChanged(s, start, before, count);
        }
    }

    /**
     * A version of the EditBookmarkTextWatcher for the keyword field of the dialog.
     * Checks if the field has any (non leading or trailing) spaces.
     */
    private class KeywordTextWatcher extends EditBookmarkTextWatcher {
        public KeywordTextWatcher(AlertDialog aDialog) {
            super(aDialog);
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Disable if the keyword contains spaces
            mEnabled = (s.toString().trim().indexOf(' ') == -1);
            super.onTextChanged(s, start, before, count);
       }
    }

    /**
     * Show the Edit bookmark dialog for a particular url. If the url is bookmarked multiple times
     * this will just edit the first instance it finds.
     *
     * @param url The url of the bookmark to edit. The dialog will look up other information like the id,
     *            current title, or keywords associated with this url. If the url isn't bookmarked, the
     *            dialog will fail silently. If the url is bookmarked multiple times, this will only show
     *            information about the first it finds.
     */
    public void show(final String url) {
        (new UiAsyncTask<Void, Void, Bookmark>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public Bookmark doInBackground(Void... params) {
                Cursor cursor = BrowserDB.getBookmarkForUrl(mContext.getContentResolver(), url);
                if (cursor == null) {
                    return null;
                }

                Bookmark bookmark = null;
                try {
                    cursor.moveToFirst();
                    bookmark = new Bookmark(cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID)),
                                                          cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.TITLE)),
                                                          cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL)),
                                                          cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.KEYWORD)));
                } finally {
                    cursor.close();
                }
                return bookmark;
            }

            @Override
            public void onPostExecute(Bookmark bookmark) {
                if (bookmark == null)
                    return;

                show(bookmark.id, bookmark.title, bookmark.url, bookmark.keyword);
            }
        }).execute();
    }

    /**
     * Show the Edit bookmark dialog for a set of data. This will show the dialog whether
     * a bookmark with this url exists or not, but the results will NOT be saved if the id
     * is not a valid bookmark id.
     *
     * @param id The id of the bookmark to change. If there is no bookmark with this ID, the dialog
     *           will fail silently.
     * @param title The initial title to show in the dialog
     * @param url The initial url to show in the dialog
     * @param keyword The initial keyword to show in the dialog
     */
    public void show(final int id, final String title, final String url, final String keyword) {
        AlertDialog.Builder editPrompt = new AlertDialog.Builder(mContext);
        final View editView = LayoutInflater.from(mContext).inflate(R.layout.bookmark_edit, null);
        editPrompt.setTitle(R.string.bookmark_edit_title);
        editPrompt.setView(editView);

        final EditText nameText = ((EditText) editView.findViewById(R.id.edit_bookmark_name));
        final EditText locationText = ((EditText) editView.findViewById(R.id.edit_bookmark_location));
        final EditText keywordText = ((EditText) editView.findViewById(R.id.edit_bookmark_keyword));
        nameText.setText(title);
        locationText.setText(url);
        keywordText.setText(keyword);

        editPrompt.setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int whichButton) {
                (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
                    @Override
                    public Void doInBackground(Void... params) {
                        String newUrl = locationText.getText().toString().trim();
                        String newKeyword = keywordText.getText().toString().trim();
                        BrowserDB.updateBookmark(mContext.getContentResolver(), id, newUrl, nameText.getText().toString(), newKeyword);
                        return null;
                    }

                    @Override
                    public void onPostExecute(Void result) {
                        Toast.makeText(mContext, R.string.bookmark_updated, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
            }
        });

        editPrompt.setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int whichButton) {
                  // do nothing
              }
        });

        final AlertDialog dialog = editPrompt.create();

        // Create our TextWatchers
        LocationTextWatcher locationTextWatcher = new LocationTextWatcher(dialog);
        KeywordTextWatcher keywordTextWatcher = new KeywordTextWatcher(dialog);

        // Cross reference the TextWatchers
        locationTextWatcher.setPairedTextWatcher(keywordTextWatcher);
        keywordTextWatcher.setPairedTextWatcher(locationTextWatcher);

        // Add the TextWatcher Listeners
        locationText.addTextChangedListener(locationTextWatcher);
        keywordText.addTextChangedListener(keywordTextWatcher);

        dialog.show();
    }
}
