/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations.Key;

public class LocalUrlAnnotations implements UrlAnnotations {
    private Uri urlAnnotationsTableWithProfile;

    public LocalUrlAnnotations(final String profile) {
        urlAnnotationsTableWithProfile = DBUtils.appendProfile(profile, BrowserContract.UrlAnnotations.CONTENT_URI);
    }

    @RobocopTarget
    @Override
    public void insertAnnotation(final ContentResolver cr, final String url, final String key, final String value) {
        final long creationTime = System.currentTimeMillis();
        final ContentValues values = new ContentValues(5);
        values.put(BrowserContract.UrlAnnotations.URL, url);
        values.put(BrowserContract.UrlAnnotations.KEY, key);
        values.put(BrowserContract.UrlAnnotations.VALUE, value);
        values.put(BrowserContract.UrlAnnotations.DATE_CREATED, creationTime);
        values.put(BrowserContract.UrlAnnotations.DATE_MODIFIED, creationTime);
        cr.insert(urlAnnotationsTableWithProfile, values);
    }

    private Cursor queryByKey(final ContentResolver cr, @NonNull final Key key, @Nullable final String[] projections,
                @Nullable final String sortOrder) {
        return cr.query(urlAnnotationsTableWithProfile,
                projections,
                BrowserContract.UrlAnnotations.KEY + " = ?", new String[] { key.getDbValue() },
                sortOrder);
    }

    @Override
    public Cursor getScreenshots(ContentResolver cr) {
        return queryByKey(cr,
                Key.SCREENSHOT,
                new String[] {
                        BrowserContract.UrlAnnotations._ID,
                        BrowserContract.UrlAnnotations.URL,
                        BrowserContract.UrlAnnotations.KEY,
                        BrowserContract.UrlAnnotations.VALUE,
                        BrowserContract.UrlAnnotations.DATE_CREATED,
                },
                BrowserContract.UrlAnnotations.DATE_CREATED + " DESC");
    }

    public void insertScreenshot(final ContentResolver cr, final String pageUrl, final String screenshotPath) {
        insertAnnotation(cr, pageUrl, Key.SCREENSHOT.getDbValue(), screenshotPath);
    }
}
