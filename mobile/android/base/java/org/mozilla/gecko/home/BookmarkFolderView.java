/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.content.Context;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class BookmarkFolderView extends LinearLayout {
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
    }

    private void setTitle(String title) {
        mTitle.setText(title);
    }

        }

        mSubtitle.setVisibility(View.GONE);
    }

    public void setState(@NonNull FolderState state) {
        mIcon.setImageResource(state.image);
    }
}
