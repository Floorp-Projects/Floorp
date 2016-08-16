/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestFaviconGenerator {
    @Test
    public void testRepresentativeCharacter() {
        Assert.assertEquals("M", FaviconGenerator.getRepresentativeCharacter("https://mozilla.org"));
        Assert.assertEquals("W", FaviconGenerator.getRepresentativeCharacter("http://wikipedia.org"));
        Assert.assertEquals("P", FaviconGenerator.getRepresentativeCharacter("http://plus.google.com"));
        Assert.assertEquals("E", FaviconGenerator.getRepresentativeCharacter("https://en.m.wikipedia.org/wiki/Main_Page"));

        // Stripping common prefixes
        Assert.assertEquals("T", FaviconGenerator.getRepresentativeCharacter("http://www.theverge.com"));
        Assert.assertEquals("F", FaviconGenerator.getRepresentativeCharacter("https://m.facebook.com"));
        Assert.assertEquals("T", FaviconGenerator.getRepresentativeCharacter("https://mobile.twitter.com"));

        // Special urls
        Assert.assertEquals("?", FaviconGenerator.getRepresentativeCharacter("file:///"));
        Assert.assertEquals("S", FaviconGenerator.getRepresentativeCharacter("file:///system/"));
        Assert.assertEquals("P", FaviconGenerator.getRepresentativeCharacter("ftp://people.mozilla.org/test"));

        // No values
        Assert.assertEquals("?", FaviconGenerator.getRepresentativeCharacter(""));
        Assert.assertEquals("?", FaviconGenerator.getRepresentativeCharacter(null));

        // Rubbish
        Assert.assertEquals("Z", FaviconGenerator.getRepresentativeCharacter("zZz"));
        Assert.assertEquals("Ö", FaviconGenerator.getRepresentativeCharacter("ölkfdpou3rkjaslfdköasdfo8"));
        Assert.assertEquals("?", FaviconGenerator.getRepresentativeCharacter("_*+*'##"));
        Assert.assertEquals("ツ", FaviconGenerator.getRepresentativeCharacter("¯\\_(ツ)_/¯"));
        Assert.assertEquals("ಠ", FaviconGenerator.getRepresentativeCharacter("ಠ_ಠ Look of Disapproval"));

        // Non-ASCII
        Assert.assertEquals("Ä", FaviconGenerator.getRepresentativeCharacter("http://www.ätzend.de"));
        Assert.assertEquals("名", FaviconGenerator.getRepresentativeCharacter("http://名がドメイン.com"));
        Assert.assertEquals("C", FaviconGenerator.getRepresentativeCharacter("http://√.com"));
        Assert.assertEquals("ß", FaviconGenerator.getRepresentativeCharacter("http://ß.de"));
        Assert.assertEquals("Ԛ", FaviconGenerator.getRepresentativeCharacter("http://ԛәлп.com/")); // cyrillic

        // Punycode
        Assert.assertEquals("X", FaviconGenerator.getRepresentativeCharacter("http://xn--tzend-fra.de")); // ätzend.de
        Assert.assertEquals("X", FaviconGenerator.getRepresentativeCharacter("http://xn--V8jxj3d1dzdz08w.com")); // 名がドメイン.com

        // Numbers
        Assert.assertEquals("1", FaviconGenerator.getRepresentativeCharacter("https://www.1and1.com/"));

        // IP
        Assert.assertEquals("1", FaviconGenerator.getRepresentativeCharacter("https://192.168.0.1"));
    }

    @Test
    public void testPickColor() {
        final int color = FaviconGenerator.pickColor("http://m.facebook.com");

        // Color does not change
        for (int i = 0; i < 100; i++) {
            Assert.assertEquals(color, FaviconGenerator.pickColor("http://m.facebook.com"));
        }

        // Color is stable for "similar" hosts.
        Assert.assertEquals(color, FaviconGenerator.pickColor("https://m.facebook.com"));
        Assert.assertEquals(color, FaviconGenerator.pickColor("http://facebook.com"));
        Assert.assertEquals(color, FaviconGenerator.pickColor("http://www.facebook.com"));
        Assert.assertEquals(color, FaviconGenerator.pickColor("http://www.facebook.com/foo/bar/foobar?mobile=1"));
    }

    @Test
    public void testGeneratingFavicon() {
        final FaviconGenerator.IconWithColor iconWithColor = FaviconGenerator.generate(RuntimeEnvironment.application, "http://m.facebook.com");
        final Bitmap bitmap = iconWithColor.bitmap;

        Assert.assertNotNull(bitmap);

        final int size = RuntimeEnvironment.application.getResources().getDimensionPixelSize(R.dimen.favicon_bg);
        Assert.assertEquals(size, bitmap.getWidth());
        Assert.assertEquals(size, bitmap.getHeight());

        Assert.assertEquals(Bitmap.Config.ARGB_8888, bitmap.getConfig());
    }
}
