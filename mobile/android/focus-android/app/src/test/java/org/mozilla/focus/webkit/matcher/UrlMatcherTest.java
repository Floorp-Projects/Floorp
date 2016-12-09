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
                "bcd"
        });

        assertTrue(matcher.matches("bcd", ""));
        assertTrue(matcher.matches("abcd", ""));
        assertTrue(matcher.matches("bcde", ""));
        assertTrue(matcher.matches("abcde", ""));

        assertTrue(!matcher.matches("a", ""));
        assertTrue(!matcher.matches("b", ""));
        assertTrue(!matcher.matches("c", ""));
        assertTrue(!matcher.matches("d", ""));
        assertTrue(!matcher.matches("e", ""));
        assertTrue(!matcher.matches("abc", ""));
        assertTrue(!matcher.matches("cde", ""));
        assertTrue(!matcher.matches("dcb", ""));
        assertTrue(!matcher.matches("edcba", ""));
    }

}