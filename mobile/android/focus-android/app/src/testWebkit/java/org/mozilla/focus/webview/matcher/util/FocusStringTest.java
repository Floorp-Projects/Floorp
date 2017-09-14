package org.mozilla.focus.webview.matcher.util;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class FocusStringTest {

    @Test (expected = StringIndexOutOfBoundsException.class)
    public void outOfBounds() throws StringIndexOutOfBoundsException {
        final String fullStringRaw = "a";
        final FocusString fullString = FocusString.create(fullStringRaw);

        // Is beyond the raw input string
        fullString.charAt(1);
    }

    @Test (expected = StringIndexOutOfBoundsException.class)
    public void outofBoundsAfterSubstring() throws StringIndexOutOfBoundsException {
        final String fullStringRaw = "abcd";
        final FocusString fullString = FocusString.create(fullStringRaw);

        final FocusString substring = fullString.substring(3);
        // substring == "d"
        substring.charAt(1);
    }

    @Test (expected = StringIndexOutOfBoundsException.class)
    public void outofBoundsSubstringLarge() throws StringIndexOutOfBoundsException {
        final String fullStringRaw = "abcd";
        final FocusString fullString = FocusString.create(fullStringRaw);

        final FocusString substring = fullString.substring(5);
    }

    @Test (expected = StringIndexOutOfBoundsException.class)
    public void outofBoundsSubstringNegative() throws StringIndexOutOfBoundsException {
        final String fullStringRaw = "abcd";
        final FocusString fullString = FocusString.create(fullStringRaw);

        final FocusString substring = fullString.substring(-1);
    }


    @Test
    public void testSubstringLength() {
        final String fullStringRaw = "a";
        final FocusString fullString = FocusString.create(fullStringRaw);

        assertEquals("FocusString length must match input string length",
                fullStringRaw.length(), fullString.length());

        final FocusString sameString = fullString.substring(0);
        assertEquals("substring(0) should equal input String",
                fullStringRaw.length(), sameString.length());
        assertEquals("substring(0) should equal input String",
                fullStringRaw.charAt(0), sameString.charAt(0));


        final FocusString emptyString = fullString.substring(1);
        assertEquals("empty substring should be empty",
                0, emptyString.length());
    }

    @Test (expected = StringIndexOutOfBoundsException.class)
    public void outofBoundsAfterSubstringEmpty() throws StringIndexOutOfBoundsException {
        final String fullStringRaw = "abcd";
        final FocusString fullString = FocusString.create(fullStringRaw);

        final FocusString substring = fullString.substring(4);
        // substring == ""
        substring.charAt(0);
    }

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