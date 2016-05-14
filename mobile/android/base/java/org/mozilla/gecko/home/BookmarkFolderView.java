/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.Context;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.Set;
import java.util.TreeSet;

public class BookmarkFolderView extends LinearLayout {
    private static final Set<Integer> FOLDERS_WITH_COUNT;

    static {
        final Set<Integer> folders = new TreeSet<>();
        folders.add(BrowserContract.Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID);

        FOLDERS_WITH_COUNT = Collections.unmodifiableSet(folders);
    }

    public enum FolderState {
        /**
         * A standard folder, i.e. a folder in a list of bookmarks and folders.
         */
        FOLDER(R.drawable.folder_closed),

        /**
         * The parent folder: this indicates that you are able to return to the previous
         * folder ("Back to {name}").
         */
        PARENT(R.drawable.bookmark_folder_arrow_up),

        /**
         * The reading list smartfolder: this displays a reading list icon instead of the
         * normal folder icon.
         */
        READING_LIST(R.drawable.reading_list_folder);

        public final int image;

        FolderState(final int image) { this.image = image; }
    }

    private final TextView mTitle;
    private final TextView mSubtitle;

    private final ImageView mIcon;

    public BookmarkFolderView(Context context) {
        this(context, null);
    }

    public BookmarkFolderView(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.two_line_folder_row, this);

        mTitle = (TextView) findViewById(R.id.title);
        mSubtitle = (TextView) findViewById(R.id.subtitle);
        mIcon =  (ImageView) findViewById(R.id.icon);
    }

    public void update(String title, int folderID) {
        setTitle(title);
        setID(folderID);
    }

    private void setTitle(String title) {
        mTitle.setText(title);
    }

    private static class ItemCountUpdateTask extends UIAsyncTask.WithoutParams<Integer> {
        private final WeakReference<TextView> mTextViewReference;
        private final int mFolderID;

        public ItemCountUpdateTask(final WeakReference<TextView> textViewReference,
                                   final int folderID) {
            super(ThreadUtils.getBackgroundHandler());

            mTextViewReference = textViewReference;
            mFolderID = folderID;
        }

        @Override
        protected Integer doInBackground() {
            final TextView textView = mTextViewReference.get();

            if (textView == null) {
                return null;
            }

            final BrowserDB db = GeckoProfile.get(textView.getContext()).getDB();
            return db.getBookmarkCountForFolder(textView.getContext().getContentResolver(), mFolderID);
        }

        @Override
        protected void onPostExecute(Integer count) {
            final TextView textView = mTextViewReference.get();

            if (textView == null) {
                return;
            }

            final String text;
            if (count == 1) {
                text = textView.getContext().getResources().getString(R.string.bookmark_folder_one_item);
            } else {
                text = textView.getContext().getResources().getString(R.string.bookmark_folder_items, count);
            }

            textView.setText(text);
            textView.setVisibility(View.VISIBLE);
        }
    }

    private void setID(final int folderID) {
        if (FOLDERS_WITH_COUNT.contains(folderID)) {
            final WeakReference<TextView> subTitleReference = new WeakReference<TextView>(mSubtitle);

            new ItemCountUpdateTask(subTitleReference, folderID).execute();
        } else {
            mSubtitle.setVisibility(View.GONE);
        }
    }

    public void setState(@NonNull FolderState state) {
        mIcon.setImageResource(state.image);
    }
}
