/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ItemHandler;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.database.Cursor;

import java.util.EnumSet;

class PanelViewUrlHandler {
    private final ViewConfig mViewConfig;
    private OnUrlOpenListener mUrlOpenListener;

    public PanelViewUrlHandler(ViewConfig viewConfig) {
        mViewConfig = viewConfig;
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    public void openUrlAtPosition(Cursor cursor, int position) {
        if (cursor == null || !cursor.moveToPosition(position)) {
            throw new IllegalStateException("Couldn't move cursor to position " + position);
        }

        int urlIndex = cursor.getColumnIndexOrThrow(HomeItems.URL);
        final String url = cursor.getString(urlIndex);

        EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.noneOf(OnUrlOpenListener.Flags.class);
        if (mViewConfig.getItemHandler() == ItemHandler.INTENT) {
            flags.add(OnUrlOpenListener.Flags.OPEN_WITH_INTENT);
        }

        if (mUrlOpenListener != null) {
            mUrlOpenListener.onUrlOpen(url, flags);
        }
    }
}
