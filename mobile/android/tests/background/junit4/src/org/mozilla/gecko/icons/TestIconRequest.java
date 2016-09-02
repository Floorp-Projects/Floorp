/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestIconRequest {
    private static final String TEST_PAGE_URL = "http://www.mozilla.org";
    private static final String TEST_ICON_URL_1 = "http://www.mozilla.org/favicon.ico";
    private static final String TEST_ICON_URL_2 = "http://www.example.org/favicon.ico";

    @Test
    public void testIconHandling() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .build();

        Assert.assertEquals(0, request.getIconCount());
        Assert.assertFalse(request.hasIconDescriptors());

        request.modify()
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_1))
                .deferBuild();

        Assert.assertEquals(1, request.getIconCount());
        Assert.assertTrue(request.hasIconDescriptors());

        request.modify()
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_2))
                .deferBuild();

        Assert.assertEquals(2, request.getIconCount());
        Assert.assertTrue(request.hasIconDescriptors());

        Assert.assertEquals(TEST_ICON_URL_1, request.getBestIcon().getUrl());

        request.moveToNextIcon();

        Assert.assertEquals(1, request.getIconCount());
        Assert.assertTrue(request.hasIconDescriptors());

        Assert.assertEquals(TEST_ICON_URL_2, request.getBestIcon().getUrl());

        request.moveToNextIcon();

        Assert.assertEquals(0, request.getIconCount());
        Assert.assertFalse(request.hasIconDescriptors());
    }
}
