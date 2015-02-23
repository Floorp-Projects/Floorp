/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import org.mozilla.gecko.mozglue.RobocopTarget;

@RobocopTarget
public interface ReadingListAccessor {
    /**
     * Returns non-deleted, non-archived items.
     * Fennec doesn't currently offer a way to display archived items.
     *
     * Can return <code>null</code>.
     */
    Cursor getReadingList(ContentResolver cr);

    int getCount(ContentResolver cr);

    Cursor getReadingListUnfetched(ContentResolver cr);

    boolean isReadingListItem(ContentResolver cr, String uri);

    long addReadingListItem(ContentResolver cr, ContentValues values);
    long addBasicReadingListItem(ContentResolver cr, String url, String title);

    void updateReadingListItem(ContentResolver cr, ContentValues values);

    void removeReadingListItemWithURL(ContentResolver cr, String uri);

    void registerContentObserver(Context context, ContentObserver observer);

    void markAsRead(ContentResolver cr, long itemID);
    void updateContent(ContentResolver cr, long itemID, String resolvedTitle, String resolvedURL, String excerpt);
    void deleteItem(ContentResolver cr, long itemID);
}
