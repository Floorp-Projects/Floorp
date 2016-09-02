/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import android.graphics.Color;

import junit.framework.Assert;

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
public class TestDiskProcessor {
    private static final String PAGE_URL = "https://www.mozilla.org";
    private static final String ICON_URL = "https://www.mozilla.org/favicon.ico";

    @Test
    public void testNetworkResponseIsStoredInCache() {
        final IconRequest request = createTestRequest();
        final IconResponse response = createTestNetworkResponse();

        final DiskStorage storage = DiskStorage.get(RuntimeEnvironment.application);
        Assert.assertNull(storage.getIcon(ICON_URL));

        final Processor processor = new DiskProcessor();
        processor.process(request, response);

        Assert.assertNotNull(storage.getIcon(ICON_URL));
    }

    @Test
    public void testGeneratedResponseIsNotStored() {
        final IconRequest request = createTestRequest();
        final IconResponse response = createGeneratedResponse();

        final DiskStorage storage = DiskStorage.get(RuntimeEnvironment.application);
        Assert.assertNull(storage.getIcon(ICON_URL));

        final Processor processor = new DiskProcessor();
        processor.process(request, response);

        Assert.assertNull(storage.getIcon(ICON_URL));
    }

    @Test
    public void testNothingIsStoredIfDiskShouldBeSkipped() {
        final IconRequest request = createTestRequest()
                .modify()
                .skipDisk()
                .build();
        final IconResponse response = createTestNetworkResponse();

        final DiskStorage storage = DiskStorage.get(RuntimeEnvironment.application);
        Assert.assertNull(storage.getIcon(ICON_URL));

        final Processor processor = new DiskProcessor();
        processor.process(request, response);

        Assert.assertNull(storage.getIcon(ICON_URL));
    }

    private IconRequest createTestRequest() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl(PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(ICON_URL))
                .build();
    }

    public IconResponse createTestNetworkResponse() {
        return IconResponse.createFromNetwork(createMockedBitmap(), ICON_URL);
    }

    public IconResponse createGeneratedResponse() {
        return IconResponse.createGenerated(createMockedBitmap(), Color.WHITE);
    }

    private Bitmap createMockedBitmap() {
        final Bitmap bitmap = mock(Bitmap.class);

        doReturn(true).when(bitmap).compress(any(Bitmap.CompressFormat.class), anyInt(), any(OutputStream.class));

        return bitmap;
    }
}
