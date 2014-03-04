/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.FilterManager;
import org.mozilla.gecko.home.PanelLayout.OnItemOpenListener;

import android.database.Cursor;

import java.util.EnumSet;

class PanelViewItemHandler {
    private final ViewConfig mViewConfig;
    private OnItemOpenListener mItemOpenListener;
    private FilterManager mFilterManager;

    public PanelViewItemHandler(ViewConfig viewConfig) {
        mViewConfig = viewConfig;
    }

    public void setOnItemOpenListener(OnItemOpenListener listener) {
        mItemOpenListener = listener;
    }

    public void setFilterManager(FilterManager manager) {
        mFilterManager = manager;
    }

    /**
     * If item at this position is a back item, perform the go back action via the
     * {@code FilterManager}. Otherwise, prepare the url to be opened by the
     * {@code OnUrlOpenListener}.
     */
    public void openItemAtPosition(Cursor cursor, int position) {
        if (mFilterManager != null && mFilterManager.canGoBack()) {
            if (position == 0) {
                mFilterManager.goBack();
                return;
            }

            position--;
        }

        if (cursor == null || !cursor.moveToPosition(position)) {
            throw new IllegalStateException("Couldn't move cursor to position " + position);
        }

        int urlIndex = cursor.getColumnIndexOrThrow(HomeItems.URL);
        final String url = cursor.getString(urlIndex);

        int titleIndex = cursor.getColumnIndexOrThrow(HomeItems.TITLE);
        final String title = cursor.getString(titleIndex);

        if (mItemOpenListener != null) {
            mItemOpenListener.onItemOpen(url, title);
        }
    }
}
