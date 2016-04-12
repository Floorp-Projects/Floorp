/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.knownsites;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.helpers.AssertUtil;

@RunWith(TestRunner.class)
public class TestKnownSiteTumblr {
    /**
     * Test that the search string is a substring of some known URLs.
     */
    @Test
    public void testURLSearchString() {
        final KnownSite tumblr = new KnownSiteTumblr();
        final String searchString = tumblr.getURLSearchString();

        AssertUtil.assertContains(
                "http://contentnotifications.tumblr.com/",
                searchString);

        AssertUtil.assertContains(
                "https://contentnotifications.tumblr.com",
                searchString);

        AssertUtil.assertContains(
                "http://contentnotifications.tumblr.com/post/142684202402/content-notification-firefox-for-android-480",
                searchString);

        AssertUtil.assertContainsNot(
                "http://www.mozilla.org",
                searchString);
    }

    /**
     * Test that we get a feed URL for valid Medium URLs.
     */
    @Test
    public void testGettingFeedFromURL() {
        final KnownSite tumblr = new KnownSiteTumblr();

        Assert.assertEquals(
                "http://contentnotifications.tumblr.com/rss",
                tumblr.getFeedFromURL("http://contentnotifications.tumblr.com/")
        );

        Assert.assertEquals(
                "http://staff.tumblr.com/rss",
                tumblr.getFeedFromURL("https://staff.tumblr.com/post/141928246566/replies-are-back-and-the-sun-is-shining-on-the")
        );

        Assert.assertNull(tumblr.getFeedFromURL("https://www.tumblr.com"));

        Assert.assertNull(tumblr.getFeedFromURL("http://www.mozilla.org"));
    }
}
