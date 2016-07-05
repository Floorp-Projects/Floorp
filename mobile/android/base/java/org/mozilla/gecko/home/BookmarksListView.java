 /* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;
import java.util.List;

import android.util.Log;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.HeaderViewListAdapter;
import android.widget.ListAdapter;

import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.util.NetworkUtils;

/**
 * A ListView of bookmarks.
 */
public class BookmarksListView extends HomeListView
                               implements AdapterView.OnItemClickListener {
    public static final String LOGTAG = "GeckoBookmarksListView";

    public BookmarksListView(Context context) {
        this(context, null);
    }

    public BookmarksListView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.bookmarksListViewStyle);
    }

    public BookmarksListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        setOnItemClickListener(this);

        setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                final int action = event.getAction();

                // If the user hit the BACK key, try to move to the parent folder.
                if (action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                    return getBookmarksListAdapter().moveToParentFolder();
                }
                return false;
            }
        });
    }

    /**
     * Get the appropriate telemetry extra for a given folder.
     *
     * baseFolderID is the ID of the first-level folder in the parent stack, i.e. the first folder
     * that was selected from the root hierarchy (e.g. Desktop, Reading List, or any mobile first-level
     * subfolder). If the current folder is a first-level folder, then the fixed root ID may be used
     * instead.
     *
     * We use baseFolderID only to distinguish whether or not we're currently in a desktop subfolder.
     * If it isn't equal to FAKE_DESKTOP_FOLDER_ID we know we're in a mobile subfolder, or one
     * of the smartfolders.
     */
    private String getTelemetryExtraForFolder(int folderID, int baseFolderID) {
        if (folderID == Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
            return "folder_desktop";
        } else if (folderID == Bookmarks.FIXED_SCREENSHOT_FOLDER_ID) {
            return "folder_screenshots";
        } else if (folderID == Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID) {
            return "folder_reading_list";
        } else {
            // The stack depth is 2 for either the fake desktop folder, or any subfolder of mobile
            // bookmarks, we subtract these offsets so that any direct subfolder of mobile
            // has a level equal to 1. (Desktop folders will be one level deeper due to the
            // fake desktop folder, hence subtract 2.)
            if (baseFolderID == Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
                return "folder_desktop_subfolder";
            } else {
                return "folder_mobile_subfolder";
            }
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        final BookmarksListAdapter adapter = getBookmarksListAdapter();
        if (adapter.isShowingChildFolder()) {
            if (position == 0) {
                // If we tap on an opened folder, move back to parent folder.

                final List<BookmarksListAdapter.FolderInfo> parentStack = ((BookmarksListAdapter) getAdapter()).getParentStack();
                if (parentStack.size() < 2) {
                    throw new IllegalStateException("Cannot move to parent folder if we are already in the root folder");
                }

                // The first item (top of stack) is the current folder, we're returning to the next one
                BookmarksListAdapter.FolderInfo folder = parentStack.get(1);
                final int parentID = folder.id;
                final int baseFolderID;
                if (parentStack.size() > 2) {
                    baseFolderID = parentStack.get(parentStack.size() - 2).id;
                } else {
                    baseFolderID = Bookmarks.FIXED_ROOT_ID;
                }

                final String extra = getTelemetryExtraForFolder(parentID, baseFolderID);

                // Move to parent _after_ retrieving stack information
                adapter.moveToParentFolder();

                Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.LIST_ITEM, extra);
                return;
            }

            // Accounting for the folder view.
            position--;
        }

        final Cursor cursor = adapter.getCursor();
        if (cursor == null) {
            return;
        }

        cursor.moveToPosition(position);

        if (adapter.getOpenFolderType() == BookmarksListAdapter.FolderType.SCREENSHOTS) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "bookmarks-screenshot");

            final String fileUrl = "file://" + cursor.getString(cursor.getColumnIndex(BrowserContract.UrlAnnotations.VALUE));
            getOnUrlOpenListener().onUrlOpen(fileUrl, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            return;
        }

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update adapter to move to that folder
            final int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            final String folderTitle = adapter.getFolderTitle(parent.getContext(), cursor);
            adapter.moveToChildFolder(folderId, folderTitle);

            final List<BookmarksListAdapter.FolderInfo> parentStack = ((BookmarksListAdapter) getAdapter()).getParentStack();

            final int baseFolderID;
            if (parentStack.size() > 2) {
                baseFolderID = parentStack.get(parentStack.size() - 2).id;
            } else {
                baseFolderID = Bookmarks.FIXED_ROOT_ID;
            }

            final String extra = getTelemetryExtraForFolder(folderId, baseFolderID);
            Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.LIST_ITEM, extra);
        } else {
            // Otherwise, just open the URL
            final String url = cursor.getString(cursor.getColumnIndexOrThrow(Bookmarks.URL));

            final SavedReaderViewHelper rvh = SavedReaderViewHelper.getSavedReaderViewHelper(getContext());

            final String extra;
            if (rvh.isURLCached(url)) {
                extra = "bookmarks-reader";
            } else {
                extra = "bookmarks";
            }

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, extra);
            Telemetry.addToHistogram("FENNEC_LOAD_SAVED_PAGE", NetworkUtils.isConnected(getContext()) ? 2 : 3);

            // This item is a TwoLinePageRow, so we allow switch-to-tab.
            getOnUrlOpenListener().onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
        }
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        // Adjust the item position to account for the parent folder row that is inserted
        // at the top of the list when viewing the contents of a folder.
        final BookmarksListAdapter adapter = getBookmarksListAdapter();
        if (adapter.isShowingChildFolder()) {
            position--;
        }

        // Temporarily prevent crashes until we figure out what we actually want to do here (bug 1252316).
        if (adapter.getOpenFolderType() == BookmarksListAdapter.FolderType.SCREENSHOTS) {
            return false;
        }

        return super.onItemLongClick(parent, view, position, id);
    }

    private BookmarksListAdapter getBookmarksListAdapter() {
        BookmarksListAdapter adapter;
        ListAdapter listAdapter = getAdapter();
        if (listAdapter instanceof HeaderViewListAdapter) {
            adapter = (BookmarksListAdapter) ((HeaderViewListAdapter) listAdapter).getWrappedAdapter();
        } else {
            adapter = (BookmarksListAdapter) listAdapter;
        }
        return adapter;
    }
}
