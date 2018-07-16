/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import org.junit.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.GeckoAppShell;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Iterator;

import static org.junit.Assert.fail;

@RunWith(RobolectricTestRunner.class)
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
    public void testPrivateMode() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.isPrivateMode());

        request.modify()
                .setPrivateMode(true)
                .deferBuild();

        Assert.assertTrue(request.isPrivateMode());

        request.modify()
                .setPrivateMode(false)
                .deferBuild();

        Assert.assertFalse(request.isPrivateMode());
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
    public void testSkipNetworkIf() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldSkipNetwork());

        request.modify()
                .skipNetworkIf(true)
                .deferBuild();

        Assert.assertTrue(request.shouldSkipNetwork());

        request.modify()
                .skipNetworkIf(false)
                .deferBuild();

        Assert.assertFalse(request.shouldSkipNetwork());
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
    public void testSkipMemoryIf() {
        IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .build();

        Assert.assertFalse(request.shouldSkipMemory());

        request.modify()
                .skipMemoryIf(true)
                .deferBuild();

        Assert.assertTrue(request.shouldSkipMemory());

        request.modify()
                .skipMemoryIf(false)
                .deferBuild();

        Assert.assertFalse(request.shouldSkipMemory());
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

        Assert.assertEquals(112, request.getTargetSize());

        request.modify()
                .forLauncherIcon()
                .deferBuild();

        Assert.assertEquals(48, request.getTargetSize());
    }

    @Test
    public void testConcurrentAccess() {
        IconRequestBuilder builder = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL_1)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_1))
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_URL_2));

        // Call build() twice on a builder and verify that the two objects are not the same
        IconRequest request = builder.build();
        IconRequest compare = builder.build();
        Assert.assertNotSame(request, compare);
        Assert.assertNotSame(request.icons, compare.icons);

        // After building call methods on the builder and verify that the previously build object is not changed
        int iconCount = request.getIconCount();
        builder.icon(IconDescriptor.createGenericIcon(TEST_PAGE_URL_2))
                .deferBuild();
        int iconCountAfterBuild = request.getIconCount();
        Assert.assertEquals(iconCount, iconCountAfterBuild);

        // Iterate the TreeSet and call methods on the builder
        try {
            final Iterator<IconDescriptor> iterator = request.icons.iterator();
            while (iterator.hasNext()) {
                iterator.next();
                builder.icon(IconDescriptor.createGenericIcon(TEST_PAGE_URL_2))
                        .deferBuild();
            }
        } catch (Exception e) {
            e.printStackTrace();
            fail("Got exception.");
        }
    }
}
