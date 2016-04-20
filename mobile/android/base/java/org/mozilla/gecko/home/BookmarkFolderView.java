/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.content.Context;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.widget.TextView;

public class BookmarkFolderView extends TextView {
    public enum FolderState {
        /**
         * A standard folder, i.e. a folder in a list of bookmarks and folders.
         */
        FOLDER(0),

        /**
         * The parent folder: this indicates that you are able to return to the previous
         * folder ("Back to {name}").
         */
        PARENT(R.attr.parent),

        /**
         * The reading list smartfolder: this displays a reading list icon instead of the
         * normal folder icon.
         */
        READING_LIST(R.attr.reading_list);

        public final int state;

        FolderState(final int state) { this.state = state; }
    }

    private FolderState mState;

    public BookmarkFolderView(Context context) {
        super(context);
        mState = FolderState.FOLDER;
    }

    public BookmarkFolderView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mState = FolderState.FOLDER;
    }

    public BookmarkFolderView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mState = FolderState.FOLDER;
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mState != null && mState != FolderState.FOLDER) {
            mergeDrawableStates(drawableState, new int[] {  mState.state });
        }

        return drawableState;
    }

    public void setState(@NonNull FolderState state) {
        if (state != mState) {
            mState = state;
            refreshDrawableState();
        }
    }
}
