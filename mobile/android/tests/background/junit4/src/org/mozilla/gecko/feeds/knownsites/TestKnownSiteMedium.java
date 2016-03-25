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
public class TestKnownSiteMedium {
    /**
     * Test that the search string is a substring of some known URLs.
     */
    @Test
    public void testURLSearchString() {
        final KnownSite medium = new KnownSiteMedium();
        final String searchString = medium.getURLSearchString();

        AssertUtil.assertContains(
                "https://medium.com/@Antlam/",
                searchString);

        AssertUtil.assertContains(
                "https://medium.com/google-developers",
                searchString);

        AssertUtil.assertContains(
                "http://medium.com/@brandonshin/how-slackbot-forced-us-to-workout-7b4741a2de73",
                searchString
        );

        AssertUtil.assertContainsNot(
                "http://www.mozilla.org",
                searchString);
    }

    /**
     * Test that we get a feed URL for valid Medium URLs.
     */
    @Test
    public void testGettingFeedFromURL() {
        final KnownSite medium = new KnownSiteMedium();

        Assert.assertEquals(
                "https://medium.com/feed/@Antlam",
                medium.getFeedFromURL("https://medium.com/@Antlam/")
        );

        Assert.assertEquals(
                "https://medium.com/feed/google-developers",
                medium.getFeedFromURL("https://medium.com/google-developers")
        );

        Assert.assertEquals(
                "https://medium.com/feed/@brandonshin",
                medium.getFeedFromURL("http://medium.com/@brandonshin/how-slackbot-forced-us-to-workout-7b4741a2de73")
        );

        Assert.assertNull(medium.getFeedFromURL("http://www.mozilla.org"));
    }
}
