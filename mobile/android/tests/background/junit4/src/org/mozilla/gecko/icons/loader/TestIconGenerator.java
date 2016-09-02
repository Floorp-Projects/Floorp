/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.loader;

import android.graphics.Bitmap;

import org.junit.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestIconGenerator {
    @Test
    public void testNoIconIsGeneratorIfThereAreIconUrlsToLoadFrom() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png"))
                .icon(IconDescriptor.createGenericIcon(
                        "https://www.mozilla.org/media/img/favicon.52506929be4c.ico"))
                .build();

        IconLoader loader = new IconGenerator();
        IconResponse response = loader.load(request);

        Assert.assertNull(response);
    }

    @Test
    public void testIconIsGeneratedForLastUrl() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png"))
                .build();

        IconLoader loader = new IconGenerator();
        IconResponse response = loader.load(request);

        Assert.assertNotNull(response);
        Assert.assertNotNull(response.getBitmap());
    }

    @Test
    public void testRepresentativeCharacter() {
        Assert.assertEquals("M", IconGenerator.getRepresentativeCharacter("https://mozilla.org"));
        Assert.assertEquals("W", IconGenerator.getRepresentativeCharacter("http://wikipedia.org"));
        Assert.assertEquals("P", IconGenerator.getRepresentativeCharacter("http://plus.google.com"));
        Assert.assertEquals("E", IconGenerator.getRepresentativeCharacter("https://en.m.wikipedia.org/wiki/Main_Page"));

        // Stripping common prefixes
        Assert.assertEquals("T", IconGenerator.getRepresentativeCharacter("http://www.theverge.com"));
        Assert.assertEquals("F", IconGenerator.getRepresentativeCharacter("https://m.facebook.com"));
        Assert.assertEquals("T", IconGenerator.getRepresentativeCharacter("https://mobile.twitter.com"));

        // Special urls
        Assert.assertEquals("?", IconGenerator.getRepresentativeCharacter("file:///"));
        Assert.assertEquals("S", IconGenerator.getRepresentativeCharacter("file:///system/"));
        Assert.assertEquals("P", IconGenerator.getRepresentativeCharacter("ftp://people.mozilla.org/test"));

        // No values
        Assert.assertEquals("?", IconGenerator.getRepresentativeCharacter(""));
        Assert.assertEquals("?", IconGenerator.getRepresentativeCharacter(null));

        // Rubbish
        Assert.assertEquals("Z", IconGenerator.getRepresentativeCharacter("zZz"));
        Assert.assertEquals("Ö", IconGenerator.getRepresentativeCharacter("ölkfdpou3rkjaslfdköasdfo8"));
        Assert.assertEquals("?", IconGenerator.getRepresentativeCharacter("_*+*'##"));
        Assert.assertEquals("ツ", IconGenerator.getRepresentativeCharacter("¯\\_(ツ)_/¯"));
        Assert.assertEquals("ಠ", IconGenerator.getRepresentativeCharacter("ಠ_ಠ Look of Disapproval"));

        // Non-ASCII
        Assert.assertEquals("Ä", IconGenerator.getRepresentativeCharacter("http://www.ätzend.de"));
        Assert.assertEquals("名", IconGenerator.getRepresentativeCharacter("http://名がドメイン.com"));
        Assert.assertEquals("C", IconGenerator.getRepresentativeCharacter("http://√.com"));
        Assert.assertEquals("ß", IconGenerator.getRepresentativeCharacter("http://ß.de"));
        Assert.assertEquals("Ԛ", IconGenerator.getRepresentativeCharacter("http://ԛәлп.com/")); // cyrillic

        // Punycode
        Assert.assertEquals("X", IconGenerator.getRepresentativeCharacter("http://xn--tzend-fra.de")); // ätzend.de
        Assert.assertEquals("X", IconGenerator.getRepresentativeCharacter("http://xn--V8jxj3d1dzdz08w.com")); // 名がドメイン.com

        // Numbers
        Assert.assertEquals("1", IconGenerator.getRepresentativeCharacter("https://www.1and1.com/"));

        // IP
        Assert.assertEquals("1", IconGenerator.getRepresentativeCharacter("https://192.168.0.1"));
    }

    @Test
    public void testPickColor() {
        final int color = IconGenerator.pickColor("http://m.facebook.com");

        // Color does not change
        for (int i = 0; i < 100; i++) {
            Assert.assertEquals(color, IconGenerator.pickColor("http://m.facebook.com"));
        }

        // Color is stable for "similar" hosts.
        Assert.assertEquals(color, IconGenerator.pickColor("https://m.facebook.com"));
        Assert.assertEquals(color, IconGenerator.pickColor("http://facebook.com"));
        Assert.assertEquals(color, IconGenerator.pickColor("http://www.facebook.com"));
        Assert.assertEquals(color, IconGenerator.pickColor("http://www.facebook.com/foo/bar/foobar?mobile=1"));
    }

    @Test
    public void testGeneratingFavicon() {
        final IconResponse response = IconGenerator.generate(RuntimeEnvironment.application, "http://m.facebook.com");
        final Bitmap bitmap = response.getBitmap();

        Assert.assertNotNull(bitmap);

        final int size = RuntimeEnvironment.application.getResources().getDimensionPixelSize(R.dimen.favicon_bg);
        Assert.assertEquals(size, bitmap.getWidth());
        Assert.assertEquals(size, bitmap.getHeight());

        Assert.assertEquals(Bitmap.Config.ARGB_8888, bitmap.getConfig());
    }
}
