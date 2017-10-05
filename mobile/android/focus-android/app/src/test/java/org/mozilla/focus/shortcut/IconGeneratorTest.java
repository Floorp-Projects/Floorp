/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;

@RunWith(RobolectricTestRunner.class)
public class IconGeneratorTest {
    @Test
    public void testRepresentativeCharacter() {
        assertEquals("M", IconGenerator.Companion.getRepresentativeCharacter("https://mozilla.org"));
        assertEquals("W", IconGenerator.Companion.getRepresentativeCharacter("http://wikipedia.org"));
        assertEquals("P", IconGenerator.Companion.getRepresentativeCharacter("http://plus.google.com"));
        assertEquals("E", IconGenerator.Companion.getRepresentativeCharacter("https://en.m.wikipedia.org/wiki/Main_Page"));

        // Stripping common prefixes
        assertEquals("T", IconGenerator.Companion.getRepresentativeCharacter("http://www.theverge.com"));
        assertEquals("F", IconGenerator.Companion.getRepresentativeCharacter("https://m.facebook.com"));
        assertEquals("T", IconGenerator.Companion.getRepresentativeCharacter("https://mobile.twitter.com"));

        // Special urls
        assertEquals("?", IconGenerator.Companion.getRepresentativeCharacter("file:///"));
        assertEquals("S", IconGenerator.Companion.getRepresentativeCharacter("file:///system/"));
        assertEquals("P", IconGenerator.Companion.getRepresentativeCharacter("ftp://people.mozilla.org/test"));

        // No values
        assertEquals("?", IconGenerator.Companion.getRepresentativeCharacter(""));
        assertEquals("?", IconGenerator.Companion.getRepresentativeCharacter(null));

        // Rubbish
        assertEquals("Z", IconGenerator.Companion.getRepresentativeCharacter("zZz"));
        assertEquals("Ö", IconGenerator.Companion.getRepresentativeCharacter("ölkfdpou3rkjaslfdköasdfo8"));
        assertEquals("?", IconGenerator.Companion.getRepresentativeCharacter("_*+*'##"));
        assertEquals("ツ", IconGenerator.Companion.getRepresentativeCharacter("¯\\_(ツ)_/¯"));
        assertEquals("ಠ", IconGenerator.Companion.getRepresentativeCharacter("ಠ_ಠ Look of Disapproval"));

        // Non-ASCII
        assertEquals("Ä", IconGenerator.Companion.getRepresentativeCharacter("http://www.ätzend.de"));
        assertEquals("名", IconGenerator.Companion.getRepresentativeCharacter("http://名がドメイン.com"));
        assertEquals("C", IconGenerator.Companion.getRepresentativeCharacter("http://√.com"));
        assertEquals("ß", IconGenerator.Companion.getRepresentativeCharacter("http://ß.de"));
        assertEquals("Ԛ", IconGenerator.Companion.getRepresentativeCharacter("http://ԛәлп.com/")); // cyrillic

        // Punycode
        assertEquals("X", IconGenerator.Companion.getRepresentativeCharacter("http://xn--tzend-fra.de")); // ätzend.de
        assertEquals("X", IconGenerator.Companion.getRepresentativeCharacter("http://xn--V8jxj3d1dzdz08w.com")); // 名がドメイン.com

        // Numbers
        assertEquals("1", IconGenerator.Companion.getRepresentativeCharacter("https://www.1and1.com/"));

        // IP
        assertEquals("1", IconGenerator.Companion.getRepresentativeCharacter("https://192.168.0.1"));
    }
}
