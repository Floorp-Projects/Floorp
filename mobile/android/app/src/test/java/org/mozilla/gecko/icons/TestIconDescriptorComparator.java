/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import org.junit.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.TreeSet;

@RunWith(TestRunner.class)
public class TestIconDescriptorComparator {
    private static final String TEST_ICON_URL_1 = "http://www.mozilla.org/favicon.ico";
    private static final String TEST_ICON_URL_2 = "http://www.example.org/favicon.ico";
    private static final String TEST_ICON_URL_3 = "http://www.example.com/favicon.ico";

    private static final String TEST_MIME_TYPE = "image/png";
    private static final int TEST_SIZE = 32;

    @Test
    public void testIconsWithTheSameUrlAreTreatedAsEqual() {
        final IconDescriptor descriptor1 = IconDescriptor.createGenericIcon(TEST_ICON_URL_1);
        final IconDescriptor descriptor2 = IconDescriptor.createGenericIcon(TEST_ICON_URL_1);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(0, comparator.compare(descriptor1, descriptor2));
        Assert.assertEquals(0, comparator.compare(descriptor2, descriptor1));
    }

    @Test
    public void testTouchIconsAreRankedHigherThanFavicons() {
        final IconDescriptor faviconDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_1, TEST_SIZE, TEST_MIME_TYPE);
        final IconDescriptor touchIconDescriptor = IconDescriptor.createTouchicon(TEST_ICON_URL_2, TEST_SIZE, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(faviconDescriptor, touchIconDescriptor));
        Assert.assertEquals(-1, comparator.compare(touchIconDescriptor, faviconDescriptor));
    }

    @Test
    public void testFaviconsAndTouchIconsAreRankedHigherThanGenericIcons() {
        final IconDescriptor genericDescriptor = IconDescriptor.createGenericIcon(TEST_ICON_URL_1);

        final IconDescriptor faviconDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_2, TEST_SIZE, TEST_MIME_TYPE);
        final IconDescriptor touchIconDescriptor = IconDescriptor.createTouchicon(TEST_ICON_URL_3, TEST_SIZE, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(genericDescriptor, faviconDescriptor));
        Assert.assertEquals(-1, comparator.compare(faviconDescriptor, genericDescriptor));

        Assert.assertEquals(1, comparator.compare(genericDescriptor, touchIconDescriptor));
        Assert.assertEquals(-1, comparator.compare(touchIconDescriptor, genericDescriptor));
    }

    @Test
    public void testLookupIconsAreRankedHigherThanGenericIcons() {
        final IconDescriptor genericDescriptor = IconDescriptor.createGenericIcon(TEST_ICON_URL_1);
        final IconDescriptor lookupDescriptor = IconDescriptor.createLookupIcon(TEST_ICON_URL_2);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(genericDescriptor, lookupDescriptor));
        Assert.assertEquals(-1, comparator.compare(lookupDescriptor, genericDescriptor));
    }

    @Test
    public void testFaviconsAndTouchIconsAreRankedHigherThanLookupIcons() {
        final IconDescriptor lookupDescriptor = IconDescriptor.createLookupIcon(TEST_ICON_URL_1);

        final IconDescriptor faviconDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_2, TEST_SIZE, TEST_MIME_TYPE);
        final IconDescriptor touchIconDescriptor = IconDescriptor.createTouchicon(TEST_ICON_URL_3, TEST_SIZE, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(lookupDescriptor, faviconDescriptor));
        Assert.assertEquals(-1, comparator.compare(faviconDescriptor, lookupDescriptor));

        Assert.assertEquals(1, comparator.compare(lookupDescriptor, touchIconDescriptor));
        Assert.assertEquals(-1, comparator.compare(touchIconDescriptor, lookupDescriptor));
    }

    @Test
    public void testLargestIconOfSameTypeIsSelected() {
        final IconDescriptor smallDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_1, 16, TEST_MIME_TYPE);
        final IconDescriptor largeDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_2, 128, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(smallDescriptor, largeDescriptor));
        Assert.assertEquals(-1, comparator.compare(largeDescriptor, smallDescriptor));
    }

    @Test
    public void testContainerTypesArePreferred() {
        final IconDescriptor containerDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_1, TEST_SIZE, "image/x-icon");
        final IconDescriptor faviconDescriptor = IconDescriptor.createFavicon(TEST_ICON_URL_2, TEST_SIZE, "image/png");

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertEquals(1, comparator.compare(faviconDescriptor, containerDescriptor));
        Assert.assertEquals(-1, comparator.compare(containerDescriptor, faviconDescriptor));
    }

    @Test
    public void testWithNoDifferences() {
        final IconDescriptor descriptor1 = IconDescriptor.createFavicon(TEST_ICON_URL_1, TEST_SIZE, TEST_MIME_TYPE);
        final IconDescriptor descriptor2 = IconDescriptor.createFavicon(TEST_ICON_URL_2, TEST_SIZE, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();

        Assert.assertNotEquals(0, comparator.compare(descriptor1, descriptor2));
        Assert.assertNotEquals(0, comparator.compare(descriptor2, descriptor1));
    }

    @Test
    public void testWithSameObject() {
        final IconDescriptor descriptor = IconDescriptor.createTouchicon(TEST_ICON_URL_1, TEST_SIZE, TEST_MIME_TYPE);

        final IconDescriptorComparator comparator = new IconDescriptorComparator();
        Assert.assertEquals(0, comparator.compare(descriptor, descriptor));
    }

    /**
     * This test reconstructs the scenario from bug 1331808. A comparator implementation that does
     * not return a consistent order can break the implementation of remove() of the TreeSet class.
     */
    @Test
    public void testBug1331808() {
        TreeSet<IconDescriptor> set = new TreeSet<>(new IconDescriptorComparator());

        set.add(IconDescriptor.createFavicon("http://example.org/new-logo32.jpg", 0, ""));
        set.add(IconDescriptor.createTouchicon("http://example.org/new-logo57.jpg", 0, ""));
        set.add(IconDescriptor.createTouchicon("http://example.org/new-logo76.jpg", 76, ""));
        set.add(IconDescriptor.createTouchicon("http://example.org/new-logo120.jpg", 120, ""));
        set.add(IconDescriptor.createTouchicon("http://example.org/new-logo152.jpg", 114, ""));
        set.add(IconDescriptor.createFavicon("http://example.org/02.png", 32, ""));
        set.add(IconDescriptor.createFavicon("http://example.org/01.png", 192, ""));
        set.add(IconDescriptor.createTouchicon("http://example.org/03.png", 0, ""));

        for (int i = 8; i > 0; i--) {
            Assert.assertEquals("items in set before deleting: " + i, i, set.size());
            Assert.assertTrue("item removed successfully: " + i, set.remove(set.first()));
            Assert.assertEquals("items in set after deleting: " + i, i - 1, set.size());
        }
    }
}
