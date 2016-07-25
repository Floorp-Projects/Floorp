/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.util.StringUtils;

import android.database.Cursor;
import android.text.TextUtils;
import android.view.View;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ExpandableListAdapter;
import android.widget.ListAdapter;

/**
 * A ContextMenuInfo for HomeListView
 */
public class HomeContextMenuInfo extends AdapterContextMenuInfo {

    public String url;
    public String title;
    public boolean isFolder;
    public int historyId = -1;
    public int bookmarkId = -1;
    public RemoveItemType itemType = null;

    // Item type to be handled with "Remove" selection.
    public static enum RemoveItemType {
        BOOKMARKS, HISTORY
    }

    public HomeContextMenuInfo(View targetView, int position, long id) {
        super(targetView, position, id);
    }

    public boolean hasBookmarkId() {
        return bookmarkId > -1;
    }

    public boolean hasHistoryId() {
        return historyId > -1;
    }

    public boolean hasPartnerBookmarkId() {
        return bookmarkId <= BrowserContract.Bookmarks.FAKE_PARTNER_BOOKMARKS_START;
    }

    public boolean canRemove() {
        return hasBookmarkId() || hasHistoryId() || hasPartnerBookmarkId();
    }

    public String getDisplayTitle() {
        if (!TextUtils.isEmpty(title)) {
            return title;
        }
        return StringUtils.stripCommonSubdomains(StringUtils.stripScheme(url, StringUtils.UrlFlags.STRIP_HTTPS));
    }

    /**
     * Interface for creating ContextMenuInfo instances from cursors.
     */
    public interface Factory {
        public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor);
    }

    /**
     * Interface for creating ContextMenuInfo instances from ListAdapters.
     */
    public interface ListFactory extends Factory {
        public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ListAdapter adapter);
    }

    /**
     * Interface for creating ContextMenuInfo instances from ExpandableListAdapters.
     */
    public interface ExpandableFactory {
        public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ExpandableListAdapter adapter);
    }
}
