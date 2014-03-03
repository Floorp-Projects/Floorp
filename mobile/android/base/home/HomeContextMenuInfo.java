/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.util.StringUtils;

import android.text.TextUtils;
import android.view.View;
import android.widget.AdapterView.AdapterContextMenuInfo;


/**
 * A ContextMenuInfo for HomeListView
 */
public class HomeContextMenuInfo extends AdapterContextMenuInfo {

    public String url;
    public String title;
    public boolean isFolder = false;
    public int display = Combined.DISPLAY_NORMAL;
    public int historyId = -1;
    public int bookmarkId = -1;
    public int readingListItemId = -1;

    public HomeContextMenuInfo(View targetView, int position, long id) {
        super(targetView, position, id);
    }

    public boolean hasBookmarkId() {
        return bookmarkId > -1;
    }

    public boolean hasHistoryId() {
        return historyId > -1;
    }

    public boolean isInReadingList() {
        return readingListItemId > -1;
    }

    public boolean canRemove() {
        return hasBookmarkId() || hasHistoryId() || isInReadingList();
    }

    public String getDisplayTitle() {
        if (!TextUtils.isEmpty(title)) {
            return title;
        }
        return StringUtils.stripCommonSubdomains(StringUtils.stripScheme(url, StringUtils.UrlFlags.STRIP_HTTPS));
    }
}