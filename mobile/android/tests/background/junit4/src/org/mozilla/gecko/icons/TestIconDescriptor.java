/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

@RunWith(TestRunner.class)
public class TestIconDescriptor {
    private static final String ICON_URL = "https://www.mozilla.org/favicon.ico";
    private static final String MIME_TYPE = "image/png";
    private static final int ICON_SIZE = 64;

    @Test
    public void testGenericIconDescriptor() {
        final IconDescriptor descriptor = IconDescriptor.createGenericIcon(ICON_URL);

        Assert.assertEquals(ICON_URL, descriptor.getUrl());
        Assert.assertNull(descriptor.getMimeType());
        Assert.assertEquals(0, descriptor.getSize());
        Assert.assertEquals(IconDescriptor.TYPE_GENERIC, descriptor.getType());
    }

    @Test
    public void testFaviconIconDescriptor() {
        final IconDescriptor descriptor = IconDescriptor.createFavicon(ICON_URL, ICON_SIZE, MIME_TYPE);

        Assert.assertEquals(ICON_URL, descriptor.getUrl());
        Assert.assertEquals(MIME_TYPE, descriptor.getMimeType());
        Assert.assertEquals(ICON_SIZE, descriptor.getSize());
        Assert.assertEquals(IconDescriptor.TYPE_FAVICON, descriptor.getType());
    }

    @Test
    public void testTouchIconDescriptor() {
        final IconDescriptor descriptor = IconDescriptor.createTouchicon(ICON_URL, ICON_SIZE, MIME_TYPE);

        Assert.assertEquals(ICON_URL, descriptor.getUrl());
        Assert.assertEquals(MIME_TYPE, descriptor.getMimeType());
        Assert.assertEquals(ICON_SIZE, descriptor.getSize());
        Assert.assertEquals(IconDescriptor.TYPE_TOUCHICON, descriptor.getType());
    }

    @Test
    public void testLookupIconDescriptor() {
        final IconDescriptor descriptor = IconDescriptor.createLookupIcon(ICON_URL);

        Assert.assertEquals(ICON_URL, descriptor.getUrl());
        Assert.assertNull(descriptor.getMimeType());
        Assert.assertEquals(0, descriptor.getSize());
        Assert.assertEquals(IconDescriptor.TYPE_LOOKUP, descriptor.getType());
    }
}
