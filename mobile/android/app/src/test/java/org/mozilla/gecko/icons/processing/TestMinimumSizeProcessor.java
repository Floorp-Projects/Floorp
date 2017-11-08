/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.robolectric.RuntimeEnvironment;

import static org.mockito.Mockito.*;

@RunWith(TestRunner.class)
public class TestMinimumSizeProcessor {

    private MinimumSizeProcessor processor;

    @Before
    public void setUp() {
        processor = new MinimumSizeProcessor();
    }

    @Test
    public void testProcessMinimumSizeZeroDoesNotReplaceSmallBitmap() throws Exception {
        final IconResponse responseMock = getMockResponse(1);
        processor.process(getMockRequest(0), responseMock);

        verify(responseMock, never()).updateBitmap(any(Bitmap.class));
        verify(responseMock, never()).updateColor(anyInt());
    }

    @Test
    public void testProcessMinimumSizeZeroDoesNotReplaceLargeBitmap() throws Exception {
        final IconResponse responseMock = getMockResponse(1000);
        processor.process(getMockRequest(0), responseMock);

        verify(responseMock, never()).updateBitmap(any(Bitmap.class));
        verify(responseMock, never()).updateColor(anyInt());
    }

    @Test
    public void testProcessMinimumSizeFiftyReplacesSmallerBitmap() throws Exception {
        final IconResponse responseMock = getMockResponse(25);
        processor.process(getMockRequest(50), responseMock);

        verify(responseMock, atLeastOnce()).updateBitmap(any(Bitmap.class));
        verify(responseMock, atLeastOnce()).updateColor(anyInt());
    }

    @Test
    public void testProcessMinimumSizeFiftyDoesNotReplaceLargerBitmap() throws Exception {
        final IconResponse responseMock = getMockResponse(1000);
        processor.process(getMockRequest(50), responseMock);

        verify(responseMock, never()).updateBitmap(any(Bitmap.class));
        verify(responseMock, never()).updateColor(anyInt());
    }

    private IconRequest getMockRequest(final int minimumSizePx) {
        final IconRequest requestMock = mock(IconRequest.class);

        // Under testing.
        when(requestMock.getMinimumSizePxAfterScaling()).thenReturn(minimumSizePx);

        // Happened to be called.
        when(requestMock.getPageUrl()).thenReturn("https://mozilla.org");
        when(requestMock.getContext()).thenReturn(RuntimeEnvironment.application);
        return requestMock;
    }

    private IconResponse getMockResponse(final int bitmapWidth) {
        final Bitmap bitmapMock = mock(Bitmap.class);
        when(bitmapMock.getWidth()).thenReturn(bitmapWidth);
        when(bitmapMock.getHeight()).thenReturn(bitmapWidth); // not strictly necessary with the current impl.

        final IconResponse responseMock = mock(IconResponse.class);
        when(responseMock.getBitmap()).thenReturn(bitmapMock);
        return responseMock;
    }
}