/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestResizingProcessor {
    private static final String PAGE_URL = "https://www.mozilla.org";
    private static final String ICON_URL = "https://www.mozilla.org/favicon.ico";

    @Test
    public void testBitmapIsNotResizedIfItAlreadyHasTheTargetSize() {
        final IconRequest request = createTestRequest();

        final Bitmap bitmap = createBitmapMock(request.getTargetSize());
        final IconResponse response = spy(IconResponse.create(bitmap));

        final ResizingProcessor processor = spy(new ResizingProcessor());
        processor.process(request, response);

        verify(processor, never()).resize(any(Bitmap.class), anyInt());
        verify(bitmap, never()).recycle();
        verify(response, never()).updateBitmap(any(Bitmap.class));
    }

    @Test
    public void testLargerBitmapsAreResized() {
        final IconRequest request = createTestRequest();

        final Bitmap bitmap = createBitmapMock(request.getTargetSize() * 2);
        final IconResponse response = spy(IconResponse.create(bitmap));

        final ResizingProcessor processor = spy(new ResizingProcessor());
        final Bitmap resizedBitmap = mock(Bitmap.class);
        doReturn(resizedBitmap).when(processor).resize(any(Bitmap.class), anyInt());
        processor.process(request, response);

        verify(processor).resize(bitmap, request.getTargetSize());
        verify(bitmap).recycle();
        verify(response).updateBitmap(resizedBitmap);
    }

    @Test
    public void testBitmapIsUpscaledToTargetSize() {
        final IconRequest request = createTestRequest();

        final Bitmap bitmap = createBitmapMock(request.getTargetSize() / 2 + 1);
        final IconResponse response = spy(IconResponse.create(bitmap));

        final ResizingProcessor processor = spy(new ResizingProcessor());
        final Bitmap resizedBitmap = mock(Bitmap.class);
        doReturn(resizedBitmap).when(processor).resize(any(Bitmap.class), anyInt());
        processor.process(request, response);

        verify(processor).resize(bitmap, request.getTargetSize());
        verify(bitmap).recycle();
        verify(response).updateBitmap(resizedBitmap);
    }

    @Test
    public void testBitmapIsNotScaledMoreThanTwoTimesTheSize() {
        final IconRequest request = createTestRequest();

        final Bitmap bitmap = createBitmapMock(5);
        final IconResponse response = spy(IconResponse.create(bitmap));

        final ResizingProcessor processor = spy(new ResizingProcessor());
        final Bitmap resizedBitmap = mock(Bitmap.class);
        doReturn(resizedBitmap).when(processor).resize(any(Bitmap.class), anyInt());
        processor.process(request, response);

        verify(processor).resize(bitmap, 10);
        verify(bitmap).recycle();
        verify(response).updateBitmap(resizedBitmap);
    }

    private IconRequest createTestRequest() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl(PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(ICON_URL))
                .build();
    }

    private Bitmap createBitmapMock(int size) {
        final Bitmap bitmap = mock(Bitmap.class);

        doReturn(size).when(bitmap).getWidth();
        doReturn(size).when(bitmap).getHeight();

        return bitmap;
    }
}
