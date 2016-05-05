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

    void insertReaderViewUrl(ContentResolver cr, String pageURL);
    void deleteReaderViewUrl(ContentResolver cr, String pageURL);

    /**
     * Did the user ever interact with this URL in regards to home screen shortcuts?
     *
     * @return true if the user has created a home screen shortcut or declined to create one in the
     *         past. This method will still return true if the shortcut has been removed from the
     *         home screen by the user.
     */
    boolean hasAcceptedOrDeclinedHomeScreenShortcut(ContentResolver cr, String url);

    /**
     * Insert an indication that the user has interacted with this URL in regards to home screen
     * shortcuts.
     *
     * @param hasCreatedShortCut True if a home screen shortcut has been created for this URL. False
     *                           if the user has actively declined to create a shortcut for this URL.
     */
    void insertHomeScreenShortcut(ContentResolver cr, String url, boolean hasCreatedShortCut);

    int getAnnotationCount(ContentResolver cr, BrowserContract.UrlAnnotations.Key key);
}
