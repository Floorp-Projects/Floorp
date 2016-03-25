/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.parser;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.Locale;

@RunWith(TestRunner.class)
public class TestSimpleFeedParser {
    /**
     * Parse and verify the RSS example from Wikipedia:
     * https://en.wikipedia.org/wiki/RSS#Example
     */
    @Test
    public void testRSSExample() throws Exception {
        InputStream stream = openFeed("feed_rss_wikipedia.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("RSS Title", feed.getTitle());
        Assert.assertEquals("http://www.example.com/main.html", feed.getWebsiteURL());
        Assert.assertNull(feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Example entry", item.getTitle());
        Assert.assertEquals("http://www.example.com/blog/post/1", item.getURL());
        Assert.assertEquals(1252254000000L, item.getTimestamp());
    }

    /**
     * Parse and verify the ATOM example from Wikipedia:
     * https://en.wikipedia.org/wiki/Atom_%28standard%29#Example_of_an_Atom_1.0_feed
     */
    @Test
    public void testATOMExample() throws Exception {
        InputStream stream = openFeed("feed_atom_wikipedia.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Example Feed", feed.getTitle());
        Assert.assertEquals("http://example.org/", feed.getWebsiteURL());
        Assert.assertEquals("http://example.org/feed/", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Atom-Powered Robots Run Amok", item.getTitle());
        Assert.assertEquals("http://example.org/2003/12/13/atom03.html", item.getURL());
        Assert.assertEquals(1071340202000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of a Medium feed.
     */
    @Test
    public void testMediumFeed() throws Exception {
        InputStream stream = openFeed("feed_rss_medium.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Anthony Lam on Medium", feed.getTitle());
        Assert.assertEquals("https://medium.com/@antlam?source=rss-59f49b9e4b19------2", feed.getWebsiteURL());
        Assert.assertEquals("https://medium.com/feed/@antlam", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("UX thoughts for 2016", item.getTitle());
        Assert.assertEquals("https://medium.com/@antlam/ux-thoughts-for-2016-1fc1d6e515e8?source=rss-59f49b9e4b19------2", item.getURL());
        Assert.assertEquals(1452537838000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of planet.mozilla.org ATOM feed.
     */
    @Test
    public void testPlanetMozillaATOMFeed() throws Exception {
        InputStream stream = openFeed("feed_atom_planetmozilla.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Planet Mozilla", feed.getTitle());
        Assert.assertEquals("http://planet.mozilla.org/", feed.getWebsiteURL());
        Assert.assertEquals("http://planet.mozilla.org/atom.xml", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Firefox 45.0 Beta 3 Testday, February 5th", item.getTitle());
        Assert.assertEquals("https://quality.mozilla.org/2016/01/firefox-45-0-beta-3-testday-february-5th/", item.getURL());
        Assert.assertEquals(1453819255000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of planet.mozilla.org RSS 2.0 feed.
     */
    @Test
    public void testPlanetMozillaRSS20Feed() throws Exception {
        InputStream stream = openFeed("feed_rss20_planetmozilla.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Planet Mozilla", feed.getTitle());
        Assert.assertEquals("http://planet.mozilla.org/", feed.getWebsiteURL());
        Assert.assertEquals("http://planet.mozilla.org/rss20.xml", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Aaron Klotz: Announcing Mozdbgext", item.getTitle());
        Assert.assertEquals("http://dblohm7.ca/blog/2016/01/26/announcing-mozdbgext/", item.getURL());
        Assert.assertEquals(1453837500000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of planet.mozilla.org RSS 1.0 feed.
     */
    @Test
    public void testPlanetMozillaRSS10Feed() throws Exception {
        InputStream stream = openFeed("feed_rss10_planetmozilla.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Planet Mozilla", feed.getTitle());
        Assert.assertEquals("http://planet.mozilla.org/", feed.getWebsiteURL());
        Assert.assertEquals("http://planet.mozilla.org/rss10.xml", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Aaron Klotz: Announcing Mozdbgext", item.getTitle());
        Assert.assertEquals("http://dblohm7.ca/blog/2016/01/26/announcing-mozdbgext/", item.getURL());
        Assert.assertEquals(1453837500000L, item.getTimestamp());
    }

    /**
     * Parse an verify a snapshot of a feedburner ATOM feed.
     */
    @Test
    public void testFeedburnerAtomFeed() throws Exception {
        InputStream stream = openFeed("feed_atom_feedburner.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Android Zeitgeist", feed.getTitle());
        Assert.assertEquals("http://www.androidzeitgeist.com/", feed.getWebsiteURL());
        Assert.assertEquals("http://feeds.feedburner.com/AndroidZeitgeist", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Support for restricted profiles in Firefox 42", item.getTitle());
        Assert.assertEquals("http://feedproxy.google.com/~r/AndroidZeitgeist/~3/xaSicfGuwOU/support-restricted-profiles-firefox.html", item.getURL());
        Assert.assertEquals(1442511968239L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of a Tumblr RSS feed.
     */
    @Test
    public void testTumblrRssFeed() throws Exception {
        InputStream stream = openFeed("feed_rss_tumblr.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("Tumblr Staff", feed.getTitle());
        Assert.assertEquals("http://staff.tumblr.com/", feed.getWebsiteURL());
        Assert.assertNull(feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("hardyboyscovers: Can Nancy Drew see things through and solve...", item.getTitle());
        Assert.assertEquals("http://staff.tumblr.com/post/138124026275", item.getURL());
        Assert.assertEquals(1453861812000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of a Spiegel (German news magazine) RSS feed.
     */
    @Test
    public void testSpiegelRssFeed() throws Exception {
        InputStream stream = openFeed("feed_rss_spon.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("SPIEGEL ONLINE - Schlagzeilen", feed.getTitle());
        Assert.assertEquals("http://www.spiegel.de", feed.getWebsiteURL());
        Assert.assertNull(feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Angebliche Vergewaltigung einer 13-Jährigen: Steinmeier kanzelt russischen Minister Lawrow ab", item.getTitle());
        Assert.assertEquals("http://www.spiegel.de/politik/ausland/steinmeier-kanzelt-lawrow-ab-aerger-um-angebliche-vergewaltigung-a-1074292.html#ref=rss", item.getURL());
        Assert.assertEquals(1453914976000L, item.getTimestamp());
    }

    /**
     * Parse and verify a snapshot of a Heise (German tech news) RSS feed.
     */
    @Test
    public void testHeiseRssFeed() throws Exception {
        InputStream stream = openFeed("feed_rss_heise.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("heise online News", feed.getTitle());
        Assert.assertEquals("http://www.heise.de/newsticker/", feed.getWebsiteURL());
        Assert.assertNull(feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Google: “Dramatische Verbesserungen” für Chrome in iOS", item.getTitle());
        Assert.assertEquals("http://www.heise.de/newsticker/meldung/Google-Dramatische-Verbesserungen-fuer-Chrome-in-iOS-3085808.html?wt_mc=rss.ho.beitrag.atom", item.getURL());
        Assert.assertEquals(1453915920000L, item.getTimestamp());
    }

    @Test
    public void testWordpressFeed() throws Exception {
        InputStream stream = openFeed("feed_rss_wordpress.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("justasimpletest2016", feed.getTitle());
        Assert.assertEquals("https://justasimpletest2016.wordpress.com", feed.getWebsiteURL());
        Assert.assertEquals("https://justasimpletest2016.wordpress.com/feed/", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("Hello World!", item.getTitle());
        Assert.assertEquals("https://justasimpletest2016.wordpress.com/2016/02/26/hello-world/", item.getURL());
        Assert.assertEquals(1456524466000L, item.getTimestamp());
    }

    /**
     * Parse and test a snapshot of mykzilla.blogspot.com
     */
    @Test
    public void testBloggerFeed() throws Exception {
        InputStream stream = openFeed("feed_atom_blogger.xml");

        SimpleFeedParser parser = new SimpleFeedParser();
        Feed feed = parser.parse(stream);

        Assert.assertNotNull(feed);
        Assert.assertEquals("mykzilla", feed.getTitle());
        Assert.assertEquals("http://mykzilla.blogspot.com/", feed.getWebsiteURL());
        Assert.assertEquals("http://www.blogger.com/feeds/18929277/posts/default", feed.getFeedURL());
        Assert.assertTrue(feed.isSufficientlyComplete());

        Item item = feed.getLastItem();

        Assert.assertNotNull(item);
        Assert.assertEquals("URL Has Been Changed", item.getTitle());
        Assert.assertEquals("http://mykzilla.blogspot.com/2016/01/url-has-been-changed.html", item.getURL());
        Assert.assertEquals(1452531451366L, item.getTimestamp());
    }

    private InputStream openFeed(String fileName) throws URISyntaxException, FileNotFoundException, UnsupportedEncodingException {
        URL url = getClass().getResource("/" + fileName);
        if (url == null) {
            throw new FileNotFoundException(fileName);
        }

        return new BufferedInputStream(new FileInputStream(url.getPath()));
    }
}
