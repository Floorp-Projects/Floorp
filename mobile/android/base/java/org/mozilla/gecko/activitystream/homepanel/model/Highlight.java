/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.model;

import android.database.Cursor;
import android.support.annotation.Nullable;
import android.text.format.DateUtils;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.ranking.HighlightCandidateCursorIndices;

public class Highlight implements Item {
    private final String title;
    private final String url;
    private final Utils.HighlightSource source;
    private final long time;

    private long historyId;

    private Metadata metadata;

    private @Nullable Boolean isPinned;
    private @Nullable Boolean isBookmarked;

    public static Highlight fromCursor(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices) {
        return new Highlight(cursor, cursorIndices);
    }

    private Highlight(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices) {
        title = cursor.getString(cursorIndices.titleColumnIndex);
        url = cursor.getString(cursorIndices.urlColumnIndex);
        source = Utils.highlightSource(cursor, cursorIndices);
        time = cursor.getLong(cursorIndices.highlightsDateColumnIndex);

        historyId = cursor.getLong(cursorIndices.historyIDColumnIndex);

        metadata = Metadata.fromCursor(cursor, cursorIndices);

        updateState();
    }

    private void updateState() {
        // We can only be certain of bookmark state if an item is a bookmark item.
        // Otherwise, due to the underlying highlights query, we have to look up states when
        // menus are displayed.
        switch (source) {
            case BOOKMARKED:
                isBookmarked = true;
                isPinned = null;
                break;
            case VISITED:
                isBookmarked = null;
                isPinned = null;
                break;
            default:
                throw new IllegalArgumentException("Unknown source: " + source);
        }
    }

    public String getTitle() {
        return title;
    }

    public String getUrl() {
        return url;
    }

    public Metadata getMetadata() {
        return metadata;
    }

    public Boolean isBookmarked() {
        return isBookmarked;
    }

    public Boolean isPinned() {
        return isPinned;
    }

    @Override
    public void updateBookmarked(boolean bookmarked) {
        this.isBookmarked = bookmarked;
    }

    @Override
    public void updatePinned(boolean pinned) {
        this.isPinned = pinned;
    }

    public String getRelativeTimeSpan() {
        return DateUtils.getRelativeTimeSpanString(
                        time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, 0).toString();
    }

    public Utils.HighlightSource getSource() {
        return source;
    }

    public long getHistoryId() {
        return historyId;
    }
}
