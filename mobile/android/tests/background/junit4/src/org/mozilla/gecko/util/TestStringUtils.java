/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.Arrays;
import java.util.Collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestStringUtils {
    @Test
    public void testIsHttpOrHttps() {
        // No value
        assertFalse(StringUtils.isHttpOrHttps(null));
        assertFalse(StringUtils.isHttpOrHttps(""));

        // Garbage
        assertFalse(StringUtils.isHttpOrHttps("lksdjflasuf"));

        // URLs with http/https
        assertTrue(StringUtils.isHttpOrHttps("https://www.google.com"));
        assertTrue(StringUtils.isHttpOrHttps("http://www.facebook.com"));
        assertTrue(StringUtils.isHttpOrHttps("https://mozilla.org/en-US/firefox/products/"));

        // IP addresses
        assertTrue(StringUtils.isHttpOrHttps("https://192.168.0.1"));
        assertTrue(StringUtils.isHttpOrHttps("http://63.245.215.20/en-US/firefox/products"));

        // Other protocols
        assertFalse(StringUtils.isHttpOrHttps("ftp://people.mozilla.org"));
        assertFalse(StringUtils.isHttpOrHttps("javascript:window.google.com"));
        assertFalse(StringUtils.isHttpOrHttps("tel://1234567890"));

        // No scheme
        assertFalse(StringUtils.isHttpOrHttps("google.com"));
        assertFalse(StringUtils.isHttpOrHttps("git@github.com:mozilla/gecko-dev.git"));
    }

    @Test
    public void testStripRef() {
        assertEquals(StringUtils.stripRef(null), null);
        assertEquals(StringUtils.stripRef(""), "");

        assertEquals(StringUtils.stripRef("??AAABBBCCC"), "??AAABBBCCC");
        assertEquals(StringUtils.stripRef("https://mozilla.org"), "https://mozilla.org");
        assertEquals(StringUtils.stripRef("https://mozilla.org#BBBB"), "https://mozilla.org");
        assertEquals(StringUtils.stripRef("https://mozilla.org/#BBBB"), "https://mozilla.org/");
    }

    @Test
    public void testStripScheme() {
        assertEquals("mozilla.org", StringUtils.stripScheme("http://mozilla.org"));
        assertEquals("mozilla.org", StringUtils.stripScheme("http://mozilla.org/"));
        assertEquals("https://mozilla.org", StringUtils.stripScheme("https://mozilla.org"));
        assertEquals("https://mozilla.org", StringUtils.stripScheme("https://mozilla.org/"));
        assertEquals("mozilla.org", StringUtils.stripScheme("https://mozilla.org/", StringUtils.UrlFlags.STRIP_HTTPS));
        assertEquals("mozilla.org", StringUtils.stripScheme("https://mozilla.org", StringUtils.UrlFlags.STRIP_HTTPS));
        assertEquals("", StringUtils.stripScheme("http://"));
        assertEquals("", StringUtils.stripScheme("https://", StringUtils.UrlFlags.STRIP_HTTPS));
        // This edge case is not handled properly yet
//        assertEquals(StringUtils.stripScheme("https://"), "");
        assertEquals(null, StringUtils.stripScheme(null));
    }

    @Test
    public void testIsRTL() {
        assertFalse(StringUtils.isRTL("mozilla.org"));
        assertFalse(StringUtils.isRTL("something.عربي"));

        assertTrue(StringUtils.isRTL("عربي"));
        assertTrue(StringUtils.isRTL("عربي.org"));

        // Text with LTR mark
        assertFalse(StringUtils.isRTL("\u200EHello"));
        assertFalse(StringUtils.isRTL("\u200Eعربي"));
    }

    @Test
    public void testForceLTR() {
        assertFalse(StringUtils.isRTL(StringUtils.forceLTR("عربي")));
        assertFalse(StringUtils.isRTL(StringUtils.forceLTR("عربي.org")));

        // Strings that are already LTR are not modified
        final String someLtrString = "HelloWorld";
        assertEquals(someLtrString, StringUtils.forceLTR(someLtrString));

        // We add the LTR mark only once
        final String someRtlString = "عربي";
        assertEquals(4, someRtlString.length());
        final String forcedLtrString = StringUtils.forceLTR(someRtlString);
        assertEquals(5, forcedLtrString.length());
        final String forcedAgainLtrString = StringUtils.forceLTR(forcedLtrString);
        assertEquals(5, forcedAgainLtrString.length());
    }

    @Test
    public void testIsSearchQuery(){
        boolean any = true;
        // test trim
        assertFalse(StringUtils.isSearchQuery("",false));
        assertTrue(StringUtils.isSearchQuery("",true));

        // test space
        assertTrue(StringUtils.isSearchQuery(" apple pen ",any));
        assertTrue(StringUtils.isSearchQuery("pineapple pen",any));
        assertTrue(StringUtils.isSearchQuery(": :",any));
        assertTrue(StringUtils.isSearchQuery(". .",any));
        assertTrue(StringUtils.isSearchQuery("gcm site:stackoverflow.com",any));
        assertTrue(StringUtils.isSearchQuery("/mnt/etc/resolv.conf does not exist",true));

        // test colon
        assertFalse(StringUtils.isSearchQuery(":",any));
        assertFalse(StringUtils.isSearchQuery("site:stackoverflow.com",any));
        assertFalse(StringUtils.isSearchQuery("http:mozilla.com",any));
        assertFalse(StringUtils.isSearchQuery("http://mozilla.com",any));
        assertFalse(StringUtils.isSearchQuery("http:/mozilla.com",any));

        // test dot
        assertFalse(StringUtils.isSearchQuery(".",any));
        assertFalse(StringUtils.isSearchQuery("cd..",any));
        assertFalse(StringUtils.isSearchQuery("cd...",any));
        assertFalse(StringUtils.isSearchQuery("mozilla.com",any));


        // test ambiguous
        String ambiguous = "~!@#$%^&*()_+`34567890-=qwertyuiop[]\\QWERTYUIOP{}|asdfghjkl;'ASDFGHJKL:\"ZXCVBNM<>?zxcvbnm,./";
        ambiguous = ambiguous.replace(" ","").replace(".","").replace(":","");
        assertTrue(StringUtils.isSearchQuery(ambiguous,true));
        assertFalse(StringUtils.isSearchQuery(ambiguous,false));


    }
}
