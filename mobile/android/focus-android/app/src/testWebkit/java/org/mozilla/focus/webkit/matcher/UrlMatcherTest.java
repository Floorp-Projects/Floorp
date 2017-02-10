package org.mozilla.focus.webkit.matcher;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class UrlMatcherTest {


    @Test
    public void matches() throws Exception {
        final UrlMatcher matcher = new UrlMatcher(new String[] {
                "bcd.random"
        });

        assertTrue(matcher.matches("http://bcd.random/something", "http://mozilla.org"));
        assertTrue(matcher.matches("http://bcd.random", "http://mozilla.org"));
        assertTrue(matcher.matches("http://www.bcd.random", "http://mozilla.org"));
        assertTrue(matcher.matches("http://www.bcd.random/something", "http://mozilla.org"));
        assertTrue(matcher.matches("http://foobar.bcd.random", "http://mozilla.org"));
        assertTrue(matcher.matches("http://foobar.bcd.random/something", "http://mozilla.org"));

        assertTrue(!matcher.matches("http://other.random", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://other.random/something", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://www.other.random", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://www.other.random/something", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://bcd.specific", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://bcd.specific/something", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://www.bcd.specific", "http://mozilla.org"));
        assertTrue(!matcher.matches("http://www.bcd.specific/something", "http://mozilla.org"));
    }

}