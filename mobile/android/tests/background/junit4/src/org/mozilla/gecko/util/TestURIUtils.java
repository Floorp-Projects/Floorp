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
import java.util.HashMap;
import java.util.Map;

@RunWith(TestRunner.class)
public class TestURIUtils {

    private final String BUGZILLA_URL = "https://bugzilla.mozilla.org/enter_bug.cgi?format=guided#h=dupes%7CData%20%26%20BI%20Services%20Team%7C";

    @Test
    public void testGetHostSecondLevelDomain() throws Exception {
        assertGetHostSLD("https://www.example.com/index.html", "example");
        assertGetHostSLD("https://m.foo.com/bar/baz?noo=abc#123", "foo");
        assertGetHostSLD("https://user:pass@m.foo.com/bar/baz?noo=abc#123", "foo");
    }

    @Test
    public void testGetHostSecondLevelDomainIPv4() throws Exception {
        assertGetHostSLD("http://192.168.1.1", "192.168.1.1");
    }

    @Test
    public void testGetHostSecondLevelDomainIPv6() throws Exception {
        assertGetHostSLD("http://[3ffe:1900:4545:3:200:f8ff:fe21:67cf]", "[3ffe:1900:4545:3:200:f8ff:fe21:67cf]");
    }

    @Test(expected = URISyntaxException.class)
    public void testGetHostSecondLevelDomainNonURI() throws Exception {
        URIUtils.getHostSecondLevelDomain(RuntimeEnvironment.application, "this  -is  -not-a-uri");
    }

    @Test(expected = NullPointerException.class)
    public void testGetHostSecondLevelDomainNullContextThrows() throws Exception {
        URIUtils.getHostSecondLevelDomain(null, "http://google.com");
    }

    @Test(expected = NullPointerException.class)
    public void testGetHostSecondLevelDomainNullURIThrows() throws Exception {
        URIUtils.getHostSecondLevelDomain(RuntimeEnvironment.application, null);
    }

    // SLD = second level domain.
    private void assertGetHostSLD(final String input, final String expected) throws Exception {
        Assert.assertEquals("for input:" + input + "||", expected,
                URIUtils.getHostSecondLevelDomain(RuntimeEnvironment.application, input));
    }

    @Test
    public void testGetBaseDomainNormal() throws Exception {
        assertGetBaseDomain("http://bbc.co.uk", "bbc.co.uk");
    }

    @Test
    public void testGetBaseDomainNormalWithAdditionalSubdomain() throws Exception {
        assertGetBaseDomain("http://a.bbc.co.uk", "bbc.co.uk");
        assertGetBaseDomain(BUGZILLA_URL, "mozilla.org");
    }

    @Test
    public void testGetBaseDomainWilcardDomain() throws Exception {
        // TLD entry: *.kawasaki.jp
        assertGetBaseDomain("http://a.b.kawasaki.jp", "a.b.kawasaki.jp");
    }

    @Test
    public void testGetBaseDomainWilcardDomainWithAdditionalSubdomain() throws Exception {
        // TLD entry: *.kawasaki.jp
        assertGetBaseDomain("http://a.b.c.kawasaki.jp", "b.c.kawasaki.jp");
    }

    @Test
    public void testGetBaseDomainExceptionDomain() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetBaseDomain("http://city.kawasaki.jp", "city.kawasaki.jp");
    }

    @Test
    public void testGetBaseDomainExceptionDomainWithAdditionalSubdomain() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetBaseDomain("http://a.city.kawasaki.jp", "city.kawasaki.jp");
    }

    @Test
    public void testGetBaseDomainExceptionDomainBugzillaURL() throws Exception {
        // TLD entry: !city.kawasaki.jp
        assertGetBaseDomain("http://a.city.kawasaki.jp", "city.kawasaki.jp");
    }

    @Test
    public void testGetBaseDomainIPv4() throws Exception {
        assertGetBaseDomain("http://192.168.1.1", null);
    }

    @Test
    public void testGetBaseDomainIPv6() throws Exception {
        assertGetBaseDomain("http://[3ffe:1900:4545:3:200:f8ff:fe21:67cf]", null);
    }

    private void assertGetBaseDomain(final String input, final String expected) throws Exception {
        Assert.assertEquals("for input:" + input + "||",
                expected,
                URIUtils.getBaseDomain(RuntimeEnvironment.application, new URI(input)));
    }
}