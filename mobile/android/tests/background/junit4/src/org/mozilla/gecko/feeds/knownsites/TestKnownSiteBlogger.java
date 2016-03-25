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
public class TestKnownSiteBlogger {
    /**
     * Test that the search string is a substring of some known URLs.
     */
    @Test
    public void testURLSearchString() {
        final KnownSite blogger = new KnownSiteBlogger();
        final String searchString = blogger.getURLSearchString();

        AssertUtil.assertContains(
                "http://mykzilla.blogspot.com/",
                searchString);

        AssertUtil.assertContains(
                "http://example.blogspot.com",
                searchString);

        AssertUtil.assertContains(
                "https://mykzilla.blogspot.com/2015/06/introducing-pluotsorbet.html",
                searchString);

        AssertUtil.assertContains(
                "http://android-developers.blogspot.com/2016/02/android-support-library-232.html",
                searchString);

        AssertUtil.assertContainsNot(
                "http://www.mozilla.org",
                searchString);
    }

    /**
     * Test that we get a feed URL for valid Blogger URLs.
     */
    @Test
    public void testGettingFeedFromURL() {
        final KnownSite blogger = new KnownSiteBlogger();

        Assert.assertEquals(
                "https://mykzilla.blogspot.com/feeds/posts/default",
                blogger.getFeedFromURL("http://mykzilla.blogspot.com/"));

        Assert.assertEquals(
                "https://example.blogspot.com/feeds/posts/default",
                blogger.getFeedFromURL("http://example.blogspot.com"));

        Assert.assertEquals(
                "https://mykzilla.blogspot.com/feeds/posts/default",
                blogger.getFeedFromURL("https://mykzilla.blogspot.com/2015/06/introducing-pluotsorbet.html"));

        Assert.assertEquals(
                "https://android-developers.blogspot.com/feeds/posts/default",
                blogger.getFeedFromURL("http://android-developers.blogspot.com/2016/02/android-support-library-232.html"));

        Assert.assertEquals(
                "https://example.blogspot.com/feeds/posts/default",
                blogger.getFeedFromURL("http://example.blogspot.com/2016/03/i-moved-to-example.blogspot.com"));

        Assert.assertNull(blogger.getFeedFromURL("http://www.mozilla.org"));
    }
}
