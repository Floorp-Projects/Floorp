/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.loader;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.icons.storage.DiskStorage;
import org.robolectric.RuntimeEnvironment;

import java.io.OutputStream;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

@RunWith(TestRunner.class)
public class TestDiskLoader {
    private static final String TEST_PAGE_URL = "http://www.mozilla.org";
    private static final String TEST_ICON_URL = "https://example.org/favicon.ico";

    @Test
    public void testLoadingFromEmptyCache() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL))
                .build();

        final IconLoader loader = new DiskLoader();
        final IconResponse response = loader.load(request);

        Assert.assertNull(response);
    }

    @Test
    public void testLoadingAfterAddingEntry() {
        final Bitmap bitmap = createMockedBitmap();
        final IconResponse originalResponse = IconResponse.createFromNetwork(bitmap, TEST_ICON_URL);

        DiskStorage.get(RuntimeEnvironment.application)
                .putIcon(originalResponse);

        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL))
                .build();

        final IconLoader loader = new DiskLoader();
        final IconResponse loadedResponse = loader.load(request);

        Assert.assertNotNull(loadedResponse);

        // The responses are not the same: The original response was stored to disk and loaded from
        // disk again. It's a copy effectively.
        Assert.assertNotEquals(originalResponse, loadedResponse);
    }

    @Test
    public void testNothingIsLoadedIfDiskShouldBeSkipped() {
        final Bitmap bitmap = createMockedBitmap();
        final IconResponse originalResponse = IconResponse.createFromNetwork(bitmap, TEST_ICON_URL);

        DiskStorage.get(RuntimeEnvironment.application)
                .putIcon(originalResponse);

        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL))
                .skipDisk()
                .build();

        final IconLoader loader = new DiskLoader();
        final IconResponse loadedResponse = loader.load(request);

        Assert.assertNull(loadedResponse);
    }

    private Bitmap createMockedBitmap() {
        final Bitmap bitmap = mock(Bitmap.class);

        doReturn(true).when(bitmap).compress(any(Bitmap.CompressFormat.class), anyInt(), any(OutputStream.class));

        return bitmap;
    }
}
