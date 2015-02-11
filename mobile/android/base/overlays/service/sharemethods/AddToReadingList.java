/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.service.sharemethods;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.overlays.service.ShareData;

import static org.mozilla.gecko.db.BrowserContract.Bookmarks;

/**
 * ShareMethod to add a page to the reading list.
 *
 * Inserts the given URL/title pair into the reading list database.
 * TODO: In the event the page turns out not to be reader-mode-compatible, freezes sometimes occur
 * when we subsequently load the page in reader mode. (Bug 1044781)
 */
public class AddToReadingList extends ShareMethod {
    private static final String LOGTAG = "GeckoAddToReadingList";

    @Override
    public Result handle(ShareData shareData) {
        ContentResolver resolver = context.getContentResolver();

        LocalBrowserDB browserDB = new LocalBrowserDB(GeckoProfile.DEFAULT_PROFILE);

        ContentValues values = new ContentValues();
        values.put(Bookmarks.TITLE, shareData.title);
        values.put(Bookmarks.URL, shareData.url);

        browserDB.getReadingListAccessor().addReadingListItem(resolver, values);

        return Result.SUCCESS;
    }

    @Override
    public String getSuccessMessage() {
        return context.getResources().getString(R.string.reading_list_added);
    }

    // Unused.
    @Override
    public String getFailureMessage() {
        return null;
    }

    public AddToReadingList(Context context) {
        super(context);
    }
}
