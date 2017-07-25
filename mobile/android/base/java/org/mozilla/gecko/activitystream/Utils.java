/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.database.Cursor;
import org.mozilla.gecko.activitystream.ranking.HighlightCandidateCursorIndices;

/**
 * Various util methods and constants that are shared by different parts of Activity Stream.
 */
public class Utils {
    public enum HighlightSource {
        VISITED,
        BOOKMARKED
    }

    public static HighlightSource highlightSource(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices) {
        if (-1 != cursor.getLong(cursorIndices.bookmarkIDColumnIndex)) {
            return HighlightSource.BOOKMARKED;
        }

        if (-1 != cursor.getLong(cursorIndices.historyIDColumnIndex)) {
            return HighlightSource.VISITED;
        }

        throw new IllegalArgumentException("Unknown highlight source.");
    }
}
