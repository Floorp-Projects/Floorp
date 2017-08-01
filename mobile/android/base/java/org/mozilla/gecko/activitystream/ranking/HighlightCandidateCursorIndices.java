/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import org.mozilla.gecko.db.BrowserContract;

/**
 * A cache of the column indices of the given Cursor.
 *
 * We use this for performance reasons: {@link Cursor#getColumnIndexOrThrow(String)} will do a HashMap look-up and
 * String comparison each time it's called, which gets expensive while iterating through HighlightCandidate results
 * (currently a maximum of 500 items), so we cache the values.
 */
public class HighlightCandidateCursorIndices {

    public final int titleColumnIndex;
    public final int urlColumnIndex;
    public final int visitsColumnIndex;
    public final int metadataColumnIndex;

    public final int highlightsDateColumnIndex;
    public final int bookmarkDateCreatedColumnIndex;
    public final int historyDateLastVisitedColumnIndex;

    public final int historyIDColumnIndex;
    public final int bookmarkIDColumnIndex;

    public HighlightCandidateCursorIndices(final Cursor cursor) {
        titleColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.History.TITLE);
        urlColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL);
        visitsColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.History.VISITS);
        metadataColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Highlights.METADATA);

        highlightsDateColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Highlights.DATE);
        bookmarkDateCreatedColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.DATE_CREATED);
        historyDateLastVisitedColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.History.DATE_LAST_VISITED);

        historyIDColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Highlights.HISTORY_ID);
        bookmarkIDColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Highlights.BOOKMARK_ID);
    }
}
