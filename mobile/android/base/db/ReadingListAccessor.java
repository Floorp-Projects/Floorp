/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;

public interface ReadingListAccessor {
    /**
     * Can return <code>null</code>.
     */
    Cursor getReadingList(ContentResolver cr);

    int getCount(ContentResolver cr);

    Cursor getReadingListUnfetched(ContentResolver cr);

    boolean isReadingListItem(ContentResolver cr, String uri);

    void addReadingListItem(ContentResolver cr, ContentValues values);

    void updateReadingListItem(ContentResolver cr, ContentValues values);

    void removeReadingListItemWithURL(ContentResolver cr, String uri);

    void registerContentObserver(Context context, ContentObserver observer);
}
