/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.preparation;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.icons.storage.DiskStorage;
import org.mozilla.gecko.icons.storage.MemoryStorage;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestLookupIconUrl {
    private static final String TEST_PAGE_URL = "http://www.mozilla.org";

    private static final String TEST_ICON_URL_1 = "http://www.mozilla.org/favicon.ico";
    private static final String TEST_ICON_URL_2 = "http://example.org/favicon.ico";
    private static final String TEST_ICON_URL_3 = "http://example.com/favicon.ico";
    private static final String TEST_ICON_URL_4 = "http://example.net/favicon.ico";


    @Before
    public void setUp() {
        MemoryStorage.get().evictAll();
        DiskStorage.get(RuntimeEnvironment.application).evictAll();
    }

    @Test
    public void testNoIconUrlIsAddedByDefault() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .build();

        Assert.assertEquals(0, request.getIconCount());

        Preparer preparer = new LookupIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(0, request.getIconCount());
    }

    @Test
    public void testIconUrlIsAddedFromMemory() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .build();

        MemoryStorage.get().putMapping(request, TEST_ICON_URL_1);

        Assert.assertEquals(0, request.getIconCount());

        Preparer preparer = new LookupIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(1, request.getIconCount());

        Assert.assertEquals(TEST_ICON_URL_1, request.getBestIcon().getUrl());
    }

    @Test
    public void testIconUrlIsAddedFromDisk() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .build();

        DiskStorage.get(RuntimeEnvironment.application).putMapping(request, TEST_ICON_URL_2);

        Assert.assertEquals(0, request.getIconCount());

        Preparer preparer = new LookupIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(1, request.getIconCount());

        Assert.assertEquals(TEST_ICON_URL_2, request.getBestIcon().getUrl());
    }

    @Test
    public void testIconUrlIsAddedFromMemoryBeforeUsingDiskStorage() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .build();

        MemoryStorage.get().putMapping(request, TEST_ICON_URL_3);
        DiskStorage.get(RuntimeEnvironment.application).putMapping(request, TEST_ICON_URL_4);

        Assert.assertEquals(0, request.getIconCount());

        Preparer preparer = new LookupIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(1, request.getIconCount());

        Assert.assertEquals(TEST_ICON_URL_3, request.getBestIcon().getUrl());
    }
}
