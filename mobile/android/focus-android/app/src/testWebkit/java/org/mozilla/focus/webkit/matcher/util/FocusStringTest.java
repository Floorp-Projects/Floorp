package org.mozilla.focus.webkit.matcher.util;

import org.junit.Test;

import static org.junit.Assert.*;

public class FocusStringTest {

    @Test
    public void testForwardString() {
        final String fullStringRaw = "abcd";
        final FocusString fullString = FocusString.create(fullStringRaw);

        assertEquals("FocusString length must match input string length",
                fullStringRaw.length(), fullString.length());

        for (int i = 0; i < fullStringRaw.length(); i++) {
            assertEquals("FocusString character doesn't match input string character",
                    fullStringRaw.charAt(i), fullString.charAt(i));
        }

        final String substringRaw = fullStringRaw.substring(2);
        final FocusString substring = fullString.substring(2);

        for (int i = 0; i < substringRaw.length(); i++) {
            assertEquals("FocusString character doesn't match input string character",
                    substringRaw.charAt(i), substring.charAt(i));
        }
    }

    @Test
    public void testReverseString() {
        final String fullUnreversedStringRaw = "abcd";

        final String fullStringRaw = new StringBuffer(fullUnreversedStringRaw).reverse().toString();
        final FocusString fullString = FocusString.create(fullUnreversedStringRaw).reverse();

        assertEquals("FocusString length must match input string length",
                fullStringRaw.length(), fullString.length());

        for (int i = 0; i < fullStringRaw.length(); i++) {
            assertEquals("FocusString character doesn't match input string character",
                    fullStringRaw.charAt(i), fullString.charAt(i));
        }

        final String substringRaw = fullStringRaw.substring(2);
        final FocusString substring = fullString.substring(2);

        for (int i = 0; i < substringRaw.length(); i++) {
            assertEquals("FocusString character doesn't match input string character",
                    substringRaw.charAt(i), substring.charAt(i));
        }
    }
}