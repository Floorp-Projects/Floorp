/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.database.Cursor;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;

public interface UrlAnnotations {
    @RobocopTarget void insertAnnotation(ContentResolver cr, String url, String key, String value);

    Cursor getScreenshots(ContentResolver cr);
    void insertScreenshot(ContentResolver cr, String pageUrl, String screenshotPath);

    Cursor getFeedSubscriptions(ContentResolver cr);
    Cursor getWebsitesWithFeedUrl(ContentResolver cr);
    void deleteFeedUrl(ContentResolver cr, String websiteUrl);
    boolean hasWebsiteForFeedUrl(ContentResolver cr, String feedUrl);
    void deleteFeedSubscription(ContentResolver cr, FeedSubscription subscription);
    void updateFeedSubscription(ContentResolver cr, FeedSubscription subscription);
    boolean hasFeedSubscription(ContentResolver cr, String feedUrl);
    void insertFeedSubscription(ContentResolver cr, FeedSubscription subscription);
    boolean hasFeedUrlForWebsite(ContentResolver cr, String websiteUrl);
    void insertFeedUrl(ContentResolver cr, String originUrl, String feedUrl);
}
