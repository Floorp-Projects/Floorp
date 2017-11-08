/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.loader;

import android.graphics.Bitmap;
import android.graphics.Color;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.icons.storage.MemoryStorage;
import org.robolectric.RuntimeEnvironment;

import static org.mockito.Mockito.mock;

@RunWith(TestRunner.class)
public class TestMemoryLoader {
    private static final String TEST_PAGE_URL = "http://www.mozilla.org";
    private static final String TEST_ICON_URL = "https://example.org/favicon.ico";

    @Before
    public void setUp() {
        // Make sure to start with an empty memory cache.
        MemoryStorage.get().evictAll();
    }

    @Test
    public void testStoringAndLoadingFromMemory() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL))
                .build();

        final IconLoader loader = new MemoryLoader();

        Assert.assertNull(loader.load(request));

        final Bitmap bitmap = mock(Bitmap.class);
        final IconResponse response = IconResponse.create(bitmap);
        response.updateColor(Color.MAGENTA);

        MemoryStorage.get().putIcon(TEST_ICON_URL, response);

        final IconResponse loadedResponse = loader.load(request);

        Assert.assertNotNull(loadedResponse);
        Assert.assertEquals(bitmap, loadedResponse.getBitmap());
        Assert.assertEquals(Color.MAGENTA, loadedResponse.getColor());
    }

    @Test
    public void testNothingIsLoadedIfMemoryShouldBeSkipped() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL))
                .skipMemory()
                .build();

        final IconLoader loader = new MemoryLoader();

        Assert.assertNull(loader.load(request));

        final Bitmap bitmap = mock(Bitmap.class);
        final IconResponse response = IconResponse.create(bitmap);

        MemoryStorage.get().putIcon(TEST_ICON_URL, response);

        Assert.assertNull(loader.load(request));
    }
}
