/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconResponse;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

@RunWith(TestRunner.class)
public class TestColorProcessor {
    @Test
    public void testExtractingColor() {
        final IconResponse response = IconResponse.create(createRedBitmapMock());

        Assert.assertFalse(response.hasColor());
        Assert.assertEquals(0, response.getColor());

        final Processor processor = new ColorProcessor();
        processor.process(null, response);

        Assert.assertTrue(response.hasColor());
        Assert.assertEquals(Color.RED, response.getColor());
    }

    private Bitmap createRedBitmapMock() {
        final Bitmap bitmap = mock(Bitmap.class);

        doReturn(1).when(bitmap).getWidth();
        doReturn(1).when(bitmap).getHeight();

        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object[] args = invocation.getArguments();
                int[] pixels = (int[]) args[0];
                for (int i = 0; i < pixels.length; i++) {
                    pixels[i] = Color.RED;
                }
                return null;
            }
        }).when(bitmap).getPixels(any(int[].class), anyInt(), anyInt(), anyInt(), anyInt(), anyInt(), anyInt());

        return bitmap;
    }
}
