/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.database.Cursor;

import org.mozilla.gecko.db.BrowserContract;

/**
 * Various util methods and constants that are shared by different parts of Activity Stream.
 */
public class Utils {
    public enum HighlightSource {
        VISITED,
        BOOKMARKED
    }

    public static HighlightSource highlightSource(final Cursor cursor) {
        if (-1 != cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.BOOKMARK_ID))) {
            return HighlightSource.BOOKMARKED;
        }

        if (-1 != cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.HISTORY_ID))) {
            return HighlightSource.VISITED;
        }

        throw new IllegalArgumentException("Unknown highlight source.");
    }
}
