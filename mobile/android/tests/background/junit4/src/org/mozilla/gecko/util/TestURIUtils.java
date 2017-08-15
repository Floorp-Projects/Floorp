/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.net.URI;
import java.net.URISyntaxException;

@RunWith(TestRunner.class)
public class TestURIUtils {

    private final String BUGZILLA_URL = "https://bugzilla.mozilla.org/enter_bug.cgi?format=guided#h=dupes%7CData%20%26%20BI%20Services%20Team%7C";

    @Test
    public void testIsPathEmptyWithURINoPath() throws Exception {
        final URI uri = new URI("https://google.com");
        Assert.assertTrue(URIUtils.isPathEmpty(uri));
    }

    @Test
    public void testIsPathEmptyWithURISlashPath() throws Exception {
        final URI uri = new URI("http://google.com/");
        Assert.assertTrue(URIUtils.isPathEmpty(uri));
    }

    @Test
    public void testIsPathEmptyWithURIDoubleSlashPath() throws Exception {
        final URI uri = new URI("http://google.com//");
        Assert.assertTrue(URIUtils.isPathEmpty(uri));
    }

    @Test
    public void testIsPathEmptyWithURIEncodedSpaceSlashPath() throws Exception {
        final URI uri = new URI("http://google.com/%20/");
        Assert.assertFalse(URIUtils.isPathEmpty(uri));
    }

    @Test
    public void testIsPathEmptyWithURIPath() throws Exception {
        final URI uri = new URI("http://google.com/search/whatever/");
        Assert.assertFalse(URIUtils.isPathEmpty(uri));
    }

    // --- getFormattedDomain, include PublicSuffix --- //
    @Test
    public void testGetFormattedDomainWithSuffix0Parts() {
        final boolean includePublicSuffix = true;
        final int subdomainCount = 0;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google.com");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "example.com");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "foo.com");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "foo.com");
    }

    @Test
    public void testGetFormattedDomainWithSuffix1Parts() {
        final boolean includePublicSuffix = true;
        final int subdomainCount = 1;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google.com");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "www.example.com");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "blog.foo.com");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.foo.com");
    }

    @Test
    public void testGetFormattedDomainWithSuffix2Parts() {
        final boolean includePublicSuffix = true;
        final int subdomainCount = 2;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google.com");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "www.example.com");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.blog.foo.com");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.foo.com");
    }

    // --- getFormattedDomain, exclude PublicSuffix --- //
    @Test
    public void testGetFormattedDomainNoSuffix0Parts() {
        final boolean includePublicSuffix = false;
        final int subdomainCount = 0;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "example");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "foo");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "foo");
    }

    @Test
    public void testGetFormattedDomainNoSuffix1Parts() {
        final boolean includePublicSuffix = false;
        final int subdomainCount = 1;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "www.example");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "blog.foo");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.foo");
    }

    @Test
    public void testGetFormattedDomainNoSuffix2Parts() {
        final boolean includePublicSuffix = false;
        final int subdomainCount = 2;
        assertGetFormattedDomain("https://google.com/search", includePublicSuffix, subdomainCount, "google");
        assertGetFormattedDomain("https://www.example.com/index.html", includePublicSuffix, subdomainCount, "www.example");
        assertGetFormattedDomain("https://m.blog.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.blog.foo");
        assertGetFormattedDomain("https://user:pass@m.foo.com/bar/baz?noo=abc#123", includePublicSuffix, subdomainCount, "m.foo");
    }

    // --- getFormattedDomain, saving time by not splitting up these tests on public suffix param. --- //
    @Test
    public void testGetFormattedDomainTwoLevelPublicSuffix() throws Exception {
        assertGetFormattedDomain("http://bbc.co.uk", false, 0, "bbc");
        assertGetFormattedDomain("http://bbc.co.uk", true, 0, "bbc.co.uk");
    }

    @Test
    public void testGetFormattedDomainNormalTwoLevelPublicSuffixWithSubdomain() throws Exception {
        assertGetFormattedDomain("http://a.bbc.co.uk", false, 0, "bbc");
        assertGetFormattedDomain("http://a.bbc.co.uk", true, 0, "bbc.co.uk");
        assertGetFormattedDomain(BUGZILLA_URL, false, 0, "mozilla");
        assertGetFormattedDomain(BUGZILLA_URL, true, 0, "mozilla.org");
    }

    @Test
    public void testGetFormattedDomainWilcardDomain() throws Exception {
        // TLD entry: *.kawasaki.jp
        assertGetFormattedDomain("http://a.b.kawasaki.jp", false, 0, "a");
        assertGetFormattedDomain("http://a.b.kawasaki.jp", true, 0, "a.b.kawasaki.jp");
    }

    @Test
    public void testGetFormattedDomainWilcardDomainWithAdditionalSubdomain() throws Exception {
        // TLD entry: *.kawasaki.jp
        assertGetFormattedDomain("http://a.b.c.kawasaki.jp", false, 0, "b");
        assertGetFormattedDomain("http://a.b.c.kawasaki.jp", true, 0, "b.c.kawasaki.jp");
    }

    @Test
    public void testGetFormattedDomainExceptionDomain() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetFormattedDomain("http://city.kawasaki.jp", false, 0, "city");
        assertGetFormattedDomain("http://city.kawasaki.jp", true, 0, "city.kawasaki.jp");
    }

    @Test
    public void testGetFormattedDomainExceptionDomainWithAdditionalSubdomain() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetFormattedDomain("http://a.city.kawasaki.jp", false, 0, "city");
        assertGetFormattedDomain("http://a.city.kawasaki.jp", true, 0, "city.kawasaki.jp");
    }

    @Test
    public void testGetFormattedDomainExceptionDomainBugzillaURL() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetFormattedDomain("http://a.city.kawasaki.jp", false, 0, "city");
        assertGetFormattedDomain("http://a.city.kawasaki.jp", true, 0, "city.kawasaki.jp");
    }

    @Test
    public void testGetFormattedDomainURIHasNoHost() throws Exception {
        assertGetFormattedDomain("file:///usr/bin", false, 0, "");
        assertGetFormattedDomain("file:///usr/bin", true, 0, "");
    }

    @Test
    public void testGetFormattedDomainIPv4() throws Exception {
        assertGetFormattedDomain("http://192.168.1.1", false, 0, "192.168.1.1");
        assertGetFormattedDomain("http://192.168.1.1", true, 0, "192.168.1.1");
    }

    @Test
    public void testGetFormattedDomainIPv6() throws Exception {
        assertGetFormattedDomain("http://[3ffe:1900:4545:3:200:f8ff:fe21:67cf]", false, 0, "[3ffe:1900:4545:3:200:f8ff:fe21:67cf]");
        assertGetFormattedDomain("http://[3ffe:1900:4545:3:200:f8ff:fe21:67cf]", true, 0, "[3ffe:1900:4545:3:200:f8ff:fe21:67cf]");
    }

    @Test(expected = NullPointerException.class)
    public void testGetFormattedDomainNullContextThrows() throws Exception {
        URIUtils.getFormattedDomain(null, new URI("http://google.com"), false, 0);
    }

    @Test(expected = NullPointerException.class)
    public void testGetFormattedDomainNullURIThrows() throws Exception {
        URIUtils.getFormattedDomain(RuntimeEnvironment.application, null, false, 0);
    }

    private void assertGetFormattedDomain(final String uriString, final boolean includePublicSuffix,
            final int subdomainCount, final String expected) {
        final URI uri;
        try {
            uri = new URI(uriString);
        } catch (final URISyntaxException e) {
            throw new IllegalArgumentException("Invalid URI passed into test: " + uriString);
        }

        Assert.assertEquals("for input:" + uriString + "||",
                expected,
               URIUtils.getFormattedDomain(RuntimeEnvironment.application, uri, includePublicSuffix, subdomainCount));
    }
}