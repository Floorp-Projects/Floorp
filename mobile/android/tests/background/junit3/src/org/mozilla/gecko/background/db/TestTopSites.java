/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;


import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.sync.setup.Constants;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;

/**
 * Exercise BrowserDB's getTopSites
 *
 * @author ahunt
 *
 */
public class TestTopSites extends ActivityInstrumentationTestCase2<Activity> {
    Context mContext;
    SuggestedSites mSuggestedSites;

    public TestTopSites() {
        super(Activity.class);
    }

    @Override
    public void setUp() {
        mContext = getInstrumentation().getTargetContext();
        mSuggestedSites = new SuggestedSites(mContext);

        // By default we're using StubBrowserDB which has no suggested sites available.
        GeckoProfile.get(mContext, Constants.DEFAULT_PROFILE).getDB().setSuggestedSites(mSuggestedSites);
    }

    @Override
    public void tearDown() {
        GeckoProfile.get(mContext, Constants.DEFAULT_PROFILE).getDB().setSuggestedSites(null);
    }

    public void testGetTopSites() {
        final int SUGGESTED_LIMIT = 6;
        final int TOTAL_LIMIT = 50;

        ContentResolver cr = mContext.getContentResolver();

        final Uri uri = BrowserContract.TopSites.CONTENT_URI
                                .buildUpon()
                                .appendQueryParameter(BrowserContract.PARAM_PROFILE,
                                                      Constants.DEFAULT_PROFILE)
                                .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                      String.valueOf(SUGGESTED_LIMIT))
                                .appendQueryParameter(BrowserContract.PARAM_SUGGESTEDSITES_LIMIT,
                                                      String.valueOf(TOTAL_LIMIT))
                                .build();

        final Cursor c = cr.query(uri,
                                  new String[] { Combined._ID,
                                          Combined.URL,
                                          Combined.TITLE,
                                          Combined.BOOKMARK_ID,
                                          Combined.HISTORY_ID },
                                  null,
                                  null,
                                  null);

        int suggestedCount = 0;
        try {
            while (c.moveToNext()) {
                int type = c.getInt(c.getColumnIndexOrThrow(BrowserContract.Bookmarks.TYPE));
                assertEquals(BrowserContract.TopSites.TYPE_SUGGESTED, type);
                suggestedCount++;
            }
        } finally {
            c.close();
        }

        Cursor suggestedSitesCursor = mSuggestedSites.get(SUGGESTED_LIMIT);

        assertEquals(suggestedSitesCursor.getCount(), suggestedCount);

        suggestedSitesCursor.close();
    }
}
