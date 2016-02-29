/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import org.mozilla.gecko.annotation.RobocopTarget;

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
}
