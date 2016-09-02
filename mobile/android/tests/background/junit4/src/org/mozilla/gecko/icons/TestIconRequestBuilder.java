/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import org.junit.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestIconRequestBuilder {
    private static final String TEST_PAGE_URL_1 = "http://www.mozilla.org";
    private static final String TEST_PAGE_URL_2 = "http://www.example.org";
    private static final String TEST_ICON_URL_1 = "http://www.mozilla.org/favicon.ico";
    private static final String TEST_ICON_URL_2 = "http://www.example.org/favicon.ico";

    @Test
    public void testPrivileged() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.isPrivileged());

        request.modify()
                .privileged(true)
                .deferBuild();

        Assert.assertTrue(request.isPrivileged());
    }

    @Test
    public void testPageUrl() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertEquals(TEST_PAGE_URL_1, request.getPageUrl());

        request.modify()
                .pageUrl(TEST_PAGE_URL_2)
                .deferBuild();

        Assert.assertEquals(TEST_PAGE_URL_2, request.getPageUrl());
    }

    @Test
    public void testIcons() {
        // Initially a request is empty.
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertEquals(0, request.getIconCount());

        // Adding one icon URL.
        request.modify()
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_1))
                .deferBuild();

        Assert.assertEquals(1, request.getIconCount());

        // Adding the same icon URL again is ignored.
        request.modify()
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_1))
                .deferBuild();

        Assert.assertEquals(1, request.getIconCount());

        // Adding another new icon URL.
        request.modify()
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_2))
                .deferBuild();

        Assert.assertEquals(2, request.getIconCount());
    }

    @Test
    public void testSkipNetwork() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldSkipNetwork());

        request.modify()
                .skipNetwork()
                .deferBuild();

        Assert.assertTrue(request.shouldSkipNetwork());
    }

    @Test
    public void testSkipDisk() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldSkipDisk());

        request.modify()
                .skipDisk()
                .deferBuild();

        Assert.assertTrue(request.shouldSkipDisk());
    }

    @Test
    public void testSkipMemory() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldSkipMemory());

        request.modify()
                .skipMemory()
                .deferBuild();

        Assert.assertTrue(request.shouldSkipMemory());
    }

    @Test
    public void testExecutionOnBackgroundThread() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldRunOnBackgroundThread());

        request.modify()
                .executeCallbackOnBackgroundThread()
                .deferBuild();

        Assert.assertTrue(request.shouldRunOnBackgroundThread());
    }

    @Test
    public void testForLauncherIcon() {
        // This code will call into GeckoAppShell to determine the launcher icon size for this configuration
        GeckoAppShell.setApplicationContext(RuntimeEnvironment.application);

        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertEquals(32, request.getTargetSize());

        request.modify()
                .forLauncherIcon()
                .deferBuild();

        Assert.assertEquals(48, request.getTargetSize());
    }
}
