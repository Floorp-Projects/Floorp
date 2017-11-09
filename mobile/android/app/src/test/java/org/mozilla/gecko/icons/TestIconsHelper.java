/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import android.annotation.SuppressLint;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.util.GeckoJarReader;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestIconsHelper {
    @SuppressLint("AuthLeak") // Lint and Android Studio try to prevent developers from writing code
                              // with credentials in the URL (user:password@host). But in this case
                              // we explicitly want to do that, so we suppress the warnings.
    @Test
    public void testGuessDefaultFaviconURL() {
        // Empty values

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL(null));
        Assert.assertNull(IconsHelper.guessDefaultFaviconURL(""));
        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("    "));

        // Special about: URLs.

        Assert.assertEquals(
                "about:home",
                IconsHelper.guessDefaultFaviconURL("about:home"));

        Assert.assertEquals(
                "about:",
                IconsHelper.guessDefaultFaviconURL("about:"));

        Assert.assertEquals(
                "about:addons",
                IconsHelper.guessDefaultFaviconURL("about:addons"));

        // Non http(s) URLS

        final String jarUrl = GeckoJarReader.getJarURL(RuntimeEnvironment.application, "chrome/chrome/content/branding/favicon64.png");
        Assert.assertEquals(jarUrl, IconsHelper.guessDefaultFaviconURL(jarUrl));

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("content://some.random.provider/icons"));

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("ftp://ftp.public.mozilla.org/this/is/made/up"));

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("file:///"));

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("file:///system/path"));

        // Various http(s) URLs

        Assert.assertEquals("http://www.mozilla.org/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("http://www.mozilla.org/"));

        Assert.assertEquals("https://www.mozilla.org/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("https://www.mozilla.org/en-US/firefox/products/"));

        Assert.assertEquals("https://example.org/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("https://example.org"));

        Assert.assertEquals("http://user:password@example.org:9991/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("http://user:password@example.org:9991/status/760492829949001728"));

        Assert.assertEquals("https://localhost:8888/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("https://localhost:8888/path/folder/file?some=query&params=none"));

        Assert.assertEquals("http://192.168.0.1/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("http://192.168.0.1/local/action.cgi"));

        Assert.assertEquals("https://medium.com/favicon.ico",
                IconsHelper.guessDefaultFaviconURL("https://medium.com/firefox-mobile-engineering/firefox-for-android-hack-week-recap-f1ab12f5cc44#.rpmzz15ia"));

        // Some broken, partial URLs

        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("http:"));
        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("http://"));
        Assert.assertNull(IconsHelper.guessDefaultFaviconURL("https:/"));
    }

    @Test
    public void testIsContainerType() {
        // Empty values
        Assert.assertFalse(IconsHelper.isContainerType(null));
        Assert.assertFalse(IconsHelper.isContainerType(""));
        Assert.assertFalse(IconsHelper.isContainerType("   "));

        // Values that don't make any sense.
        Assert.assertFalse(IconsHelper.isContainerType("Hello World"));
        Assert.assertFalse(IconsHelper.isContainerType("no/no/no"));
        Assert.assertFalse(IconsHelper.isContainerType("42"));

        // Actual image MIME types that are not container types
        Assert.assertFalse(IconsHelper.isContainerType("image/png"));
        Assert.assertFalse(IconsHelper.isContainerType("application/bmp"));
        Assert.assertFalse(IconsHelper.isContainerType("image/gif"));
        Assert.assertFalse(IconsHelper.isContainerType("image/x-windows-bitmap"));
        Assert.assertFalse(IconsHelper.isContainerType("image/jpeg"));
        Assert.assertFalse(IconsHelper.isContainerType("application/x-png"));

        // MIME types of image container
        Assert.assertTrue(IconsHelper.isContainerType("image/vnd.microsoft.icon"));
        Assert.assertTrue(IconsHelper.isContainerType("image/ico"));
        Assert.assertTrue(IconsHelper.isContainerType("image/icon"));
        Assert.assertTrue(IconsHelper.isContainerType("image/x-icon"));
        Assert.assertTrue(IconsHelper.isContainerType("text/ico"));
        Assert.assertTrue(IconsHelper.isContainerType("application/ico"));
    }

    @Test
    public void testCanDecodeType() {
        // Empty values
        Assert.assertFalse(IconsHelper.canDecodeType(null));
        Assert.assertFalse(IconsHelper.canDecodeType(""));
        Assert.assertFalse(IconsHelper.canDecodeType("   "));

        // Some things we can't decode (or that just aren't images)
        Assert.assertFalse(IconsHelper.canDecodeType("image/svg+xml"));
        Assert.assertFalse(IconsHelper.canDecodeType("video/avi"));
        Assert.assertFalse(IconsHelper.canDecodeType("text/plain"));
        Assert.assertFalse(IconsHelper.canDecodeType("image/x-quicktime"));
        Assert.assertFalse(IconsHelper.canDecodeType("image/tiff"));
        Assert.assertFalse(IconsHelper.canDecodeType("application/zip"));

        // Some image MIME types we definitely can decode
        Assert.assertTrue(IconsHelper.canDecodeType("image/bmp"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/x-icon"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/png"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/jpg"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/jpeg"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/ico"));
        Assert.assertTrue(IconsHelper.canDecodeType("image/icon"));
    }
}
