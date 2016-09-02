/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Assert;
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
public class TestMemoryProcessor {
    private static final String PAGE_URL = "https://www.mozilla.org";
    private static final String ICON_URL = "https://www.mozilla.org/favicon.ico";
    private static final String DATA_URL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAEklEQVR4AWP4z8AAxCDiP8N/AB3wBPxcBee7AAAAAElFTkSuQmCC";

    @Before
    public void setUp() {
        MemoryStorage.get().evictAll();
    }

    @Test
    public void testResponsesAreStoredInMemory() {
        final IconRequest request = createTestRequest();

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));

        final Processor processor = new MemoryProcessor();
        processor.process(request, createTestResponse());

        Assert.assertNotNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNotNull(MemoryStorage.get().getMapping(PAGE_URL));
    }

    @Test
    public void testNothingIsStoredIfMemoryShouldBeSkipped() {
        final IconRequest request = createTestRequest()
                .modify()
                .skipMemory()
                .build();

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));

        final Processor processor = new MemoryProcessor();
        processor.process(request, createTestResponse());

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));
    }

    @Test
    public void testNothingIsStoredForRequestsWithoutUrl() {
        final IconRequest request = createTestRequestWithoutIconUrl();

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));

        final Processor processor = new MemoryProcessor();
        processor.process(request, createTestResponse());

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));
    }

    @Test
    public void testNothingIsStoredForGeneratedResponses() {
        final IconRequest request = createTestRequest();

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));

        final Processor processor = new MemoryProcessor();
        processor.process(request, createGeneratedTestResponse());

        Assert.assertNull(MemoryStorage.get().getIcon(ICON_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));
    }

    @Test
    public void testNothingIsStoredForDataUris() {
        final IconRequest request = createDataUriTestRequest();

        Assert.assertNull(MemoryStorage.get().getIcon(DATA_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));

        final Processor processor = new MemoryProcessor();
        processor.process(request, createTestResponse());

        Assert.assertNull(MemoryStorage.get().getIcon(DATA_URL));
        Assert.assertNull(MemoryStorage.get().getMapping(PAGE_URL));
    }

    private IconRequest createTestRequest() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl(PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(ICON_URL))
                .build();
    }

    private IconRequest createTestRequestWithoutIconUrl() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl(PAGE_URL)
                .build();
    }

    private IconRequest createDataUriTestRequest() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl(PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(DATA_URL))
                .build();
    }

    private IconResponse createTestResponse() {
        return IconResponse.create(mock(Bitmap.class));
    }

    private IconResponse createGeneratedTestResponse() {
        return IconResponse.createGenerated(mock(Bitmap.class), Color.GREEN);
    }
}
