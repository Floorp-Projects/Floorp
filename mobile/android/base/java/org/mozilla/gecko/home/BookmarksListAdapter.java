/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.View;

/**
 * Adapter to back the BookmarksListView with a list of bookmarks.
 */
class BookmarksListAdapter extends MultiTypeCursorAdapter {
    private static final int VIEW_TYPE_BOOKMARK_ITEM = 0;
    private static final int VIEW_TYPE_FOLDER = 1;
    private static final int VIEW_TYPE_SCREENSHOT = 2;

    private static final int[] VIEW_TYPES = new int[] { VIEW_TYPE_BOOKMARK_ITEM, VIEW_TYPE_FOLDER, VIEW_TYPE_SCREENSHOT };
    private static final int[] LAYOUT_TYPES =
            new int[] { R.layout.bookmark_item_row, R.layout.bookmark_folder_row, R.layout.bookmark_screenshot_row };

    public enum RefreshType implements Parcelable {
        PARENT,
        CHILD;

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<RefreshType> CREATOR = new Creator<RefreshType>() {
            @Override
            public RefreshType createFromParcel(final Parcel source) {
                return RefreshType.values()[source.readInt()];
            }

            @Override
            public RefreshType[] newArray(final int size) {
                return new RefreshType[size];
            }
        };
    }

    public static class FolderInfo implements Parcelable {
        public final int id;
        public final String title;

        public FolderInfo(int id) {
            this(id, "");
        }

        public FolderInfo(Parcel in) {
            this(in.readInt(), in.readString());
        }

        public FolderInfo(int id, String title) {
            this.id = id;
            this.title = title;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(id);
            dest.writeString(title);
        }

        public static final Creator<FolderInfo> CREATOR = new Creator<FolderInfo>() {
            @Override
            public FolderInfo createFromParcel(Parcel in) {
                return new FolderInfo(in);
            }

            @Override
            public FolderInfo[] newArray(int size) {
                return new FolderInfo[size];
            }
        };
    }

    // A listener that knows how to refresh the list for a given folder id.
    // This is usually implemented by the enclosing fragment/activity.
    public static interface OnRefreshFolderListener {
        // The folder id to refresh the list with.
        public void onRefreshFolder(FolderInfo folderInfo, RefreshType refreshType);
    }

    /**
     * The type of data a bookmarks folder can display. This can be used to
     * distinguish bookmark folders from "smart folders" that contain non-bookmark
     * entries but still appear in the Bookmarks panel.
     */
    public enum FolderType {
        BOOKMARKS,
        SCREENSHOTS,
    }

    // mParentStack holds folder info instances (id + title) that allow
    // us to navigate back up the folder hierarchy.
    private final LinkedList<FolderInfo> mParentStack;

    // Refresh folder listener.
    private OnRefreshFolderListener mListener;

    private FolderType openFolderType = FolderType.BOOKMARKS;

    public BookmarksListAdapter(Context context, Cursor cursor, List<FolderInfo> parentStack) {
        // Initializing with a null cursor.
        super(context, cursor, VIEW_TYPES, LAYOUT_TYPES);

        if (parentStack == null) {
            mParentStack = new LinkedList<FolderInfo>();
        } else {
            mParentStack = new LinkedList<FolderInfo>(parentStack);
        }
    }

    public List<FolderInfo> getParentStack() {
        return Collections.unmodifiableList(mParentStack);
    }

    public FolderType getOpenFolderType() {
        return openFolderType;
    }

    /**
     * Moves to parent folder, if one exists.
     *
     * @return Whether the adapter successfully moved to a parent folder.
     */
    public boolean moveToParentFolder() {
        // If we're already at the root, we can't move to a parent folder.
        // An empty parent stack here means we're still waiting for the
        // initial list of bookmarks and can't go to a parent folder.
        if (mParentStack.size() <= 1) {
            return false;
        }

        if (mListener != null) {
            // We pick the second folder in the stack as it represents
            // the parent folder.
            mListener.onRefreshFolder(mParentStack.get(1), RefreshType.PARENT);
        }

        return true;
    }

    /**
     * Moves to child folder, given a folderId.
     *
     * @param folderId The id of the folder to show.
     * @param folderTitle The title of the folder to show.
     */
    public void moveToChildFolder(int folderId, String folderTitle) {
        FolderInfo folderInfo = new FolderInfo(folderId, folderTitle);

        if (mListener != null) {
            mListener.onRefreshFolder(folderInfo, RefreshType.CHILD);
        }
    }

    /**
     * Set a listener that can refresh this adapter.
     *
     * @param listener The listener that can refresh the adapter.
     */
    public void setOnRefreshFolderListener(OnRefreshFolderListener listener) {
        mListener = listener;
    }

    private boolean isCurrentFolder(FolderInfo folderInfo) {
        return (mParentStack.size() > 0 &&
                mParentStack.peek().id == folderInfo.id);
    }

    public void swapCursor(Cursor c, FolderInfo folderInfo, RefreshType refreshType) {
        updateOpenFolderType(folderInfo);
        switch(refreshType) {
            case PARENT:
                if (!isCurrentFolder(folderInfo)) {
                    mParentStack.removeFirst();
                }
                break;

            case CHILD:
                if (!isCurrentFolder(folderInfo)) {
                    mParentStack.addFirst(folderInfo);
                }
                break;

            default:
                // Do nothing;
        }

        swapCursor(c);
    }

    private void updateOpenFolderType(final FolderInfo folderInfo) {
        if (folderInfo.id == Bookmarks.FIXED_SCREENSHOT_FOLDER_ID) {
            openFolderType = FolderType.SCREENSHOTS;
        } else {
            openFolderType = FolderType.BOOKMARKS;
        }
    }

    @Override
    public int getItemViewType(int position) {
        // The position also reflects the opened child folder row.
        if (isShowingChildFolder()) {
            if (position == 0) {
                return VIEW_TYPE_FOLDER;
            }

            // Accounting for the folder view.
            position--;
        }

        if (openFolderType == FolderType.SCREENSHOTS) {
            return VIEW_TYPE_SCREENSHOT;
        }

        final Cursor c = getCursor(position);
        if (c.getInt(c.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER) {
            return VIEW_TYPE_FOLDER;
        }

        // Default to returning normal item type.
        return VIEW_TYPE_BOOKMARK_ITEM;
    }

    /**
     * Get the title of the folder given a cursor moved to the position.
     *
     * @param context The context of the view.
     * @param cursor A cursor moved to the required position.
     * @return The title of the folder at the position.
     */
    public String getFolderTitle(Context context, Cursor c) {
        String guid = c.getString(c.getColumnIndexOrThrow(Bookmarks.GUID));

        // If we don't have a special GUID, just return the folder title from the DB.
        if (guid == null || guid.length() == 12) {
            return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
        }

        Resources res = context.getResources();

        // Use localized strings for special folder names.
        if (guid.equals(Bookmarks.FAKE_DESKTOP_FOLDER_GUID)) {
            return res.getString(R.string.bookmarks_folder_desktop);
        } else if (guid.equals(Bookmarks.MENU_FOLDER_GUID)) {
            return res.getString(R.string.bookmarks_folder_menu);
        } else if (guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID)) {
            return res.getString(R.string.bookmarks_folder_toolbar);
        } else if (guid.equals(Bookmarks.UNFILED_FOLDER_GUID)) {
            return res.getString(R.string.bookmarks_folder_unfiled);
        } else if (guid.equals(Bookmarks.SCREENSHOT_FOLDER_GUID)) {
            return res.getString(R.string.screenshot_folder_label_in_bookmarks);
        }

        // If for some reason we have a folder with a special GUID, but it's not one of
        // the special folders we expect in the UI, just return the title from the DB.
        return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
    }

    /**
     * @return true, if currently showing a child folder, false otherwise.
     */
    public boolean isShowingChildFolder() {
        if (mParentStack.size() == 0) {
            return false;
        }

        return (mParentStack.peek().id != Bookmarks.FIXED_ROOT_ID);
    }

    @Override
    public int getCount() {
        return super.getCount() + (isShowingChildFolder() ? 1 : 0);
    }

    @Override
    public void bindView(View view, Context context, int position) {
        final int viewType = getItemViewType(position);

        final Cursor cursor;
        if (isShowingChildFolder()) {
            if (position == 0) {
                cursor = null;
            } else {
                // Accounting for the folder view.
                position--;
                cursor = getCursor(position);
            }
        } else {
            cursor = getCursor(position);
        }

        if (viewType == VIEW_TYPE_SCREENSHOT) {
            ((BookmarkScreenshotRow) view).updateFromCursor(cursor);
        } else if (viewType == VIEW_TYPE_BOOKMARK_ITEM) {
            final TwoLinePageRow row = (TwoLinePageRow) view;
            row.updateFromCursor(cursor);
        } else {
            final BookmarkFolderView row = (BookmarkFolderView) view;
            if (cursor == null) {
                final Resources res = context.getResources();
                row.setText(res.getString(R.string.home_move_back_to_filter, mParentStack.get(1).title));
                row.open();
            } else {
                row.setText(getFolderTitle(context, cursor));
                row.close();
            }
        }
    }
}
