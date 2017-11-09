/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.mockito.Mockito.mock;

@RunWith(TestRunner.class)
public class TestIconResponse {
    private static final String ICON_URL = "http://www.mozilla.org/favicon.ico";

    @Test
    public void testDefaultResponse() {
        final Bitmap bitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.create(bitmap);

        Assert.assertEquals(bitmap, response.getBitmap());
        Assert.assertFalse(response.hasUrl());
        Assert.assertNull(response.getUrl());

        Assert.assertFalse(response.hasColor());
        Assert.assertEquals(0, response.getColor());

        Assert.assertFalse(response.isGenerated());
        Assert.assertFalse(response.isFromNetwork());
        Assert.assertFalse(response.isFromDisk());
        Assert.assertFalse(response.isFromMemory());
    }

    @Test
    public void testNetworkResponse() {
        final Bitmap bitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.createFromNetwork(bitmap, ICON_URL);

        Assert.assertEquals(bitmap, response.getBitmap());
        Assert.assertTrue(response.hasUrl());
        Assert.assertEquals(ICON_URL, response.getUrl());

        Assert.assertFalse(response.hasColor());
        Assert.assertEquals(0, response.getColor());

        Assert.assertFalse(response.isGenerated());
        Assert.assertTrue(response.isFromNetwork());
        Assert.assertFalse(response.isFromDisk());
        Assert.assertFalse(response.isFromMemory());
    }

    @Test
    public void testGeneratedResponse() {
        final Bitmap bitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.createGenerated(bitmap, Color.CYAN);

        Assert.assertEquals(bitmap, response.getBitmap());
        Assert.assertFalse(response.hasUrl());
        Assert.assertNull(response.getUrl());

        Assert.assertTrue(response.hasColor());
        Assert.assertEquals(Color.CYAN, response.getColor());

        Assert.assertTrue(response.isGenerated());
        Assert.assertFalse(response.isFromNetwork());
        Assert.assertFalse(response.isFromDisk());
        Assert.assertFalse(response.isFromMemory());
    }

    @Test
    public void testMemoryResponse() {
        final Bitmap bitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.createFromMemory(bitmap, ICON_URL, Color.CYAN);

        Assert.assertEquals(bitmap, response.getBitmap());
        Assert.assertTrue(response.hasUrl());
        Assert.assertEquals(ICON_URL, response.getUrl());

        Assert.assertTrue(response.hasColor());
        Assert.assertEquals(Color.CYAN, response.getColor());

        Assert.assertFalse(response.isGenerated());
        Assert.assertFalse(response.isFromNetwork());
        Assert.assertFalse(response.isFromDisk());
        Assert.assertTrue(response.isFromMemory());
    }

    @Test
    public void testDiskResponse() {
        final Bitmap bitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.createFromDisk(bitmap, ICON_URL);

        Assert.assertEquals(bitmap, response.getBitmap());
        Assert.assertTrue(response.hasUrl());
        Assert.assertEquals(ICON_URL, response.getUrl());

        Assert.assertFalse(response.hasColor());
        Assert.assertEquals(0, response.getColor());

        Assert.assertFalse(response.isGenerated());
        Assert.assertFalse(response.isFromNetwork());
        Assert.assertTrue(response.isFromDisk());
        Assert.assertFalse(response.isFromMemory());
    }

    @Test
    public void testUpdatingColor() {
        final IconResponse response = IconResponse.create(mock(Bitmap.class));

        Assert.assertFalse(response.hasColor());
        Assert.assertEquals(0, response.getColor());

        response.updateColor(Color.YELLOW);

        Assert.assertTrue(response.hasColor());
        Assert.assertEquals(Color.YELLOW, response.getColor());

        response.updateColor(Color.MAGENTA);

        Assert.assertTrue(response.hasColor());
        Assert.assertEquals(Color.MAGENTA, response.getColor());
    }

    @Test
    public void testUpdatingBitmap() {
        final Bitmap originalBitmap = mock(Bitmap.class);
        final Bitmap updatedBitmap = mock(Bitmap.class);

        final IconResponse response = IconResponse.create(originalBitmap);

        Assert.assertEquals(originalBitmap, response.getBitmap());
        Assert.assertNotEquals(updatedBitmap, response.getBitmap());

        response.updateBitmap(updatedBitmap);

        Assert.assertNotEquals(originalBitmap, response.getBitmap());
        Assert.assertEquals(updatedBitmap, response.getBitmap());
    }
}
