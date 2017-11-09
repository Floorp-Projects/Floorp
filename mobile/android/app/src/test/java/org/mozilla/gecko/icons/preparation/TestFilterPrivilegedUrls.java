package org.mozilla.gecko.icons.preparation;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

import java.util.Iterator;

@RunWith(TestRunner.class)
public class TestFilterPrivilegedUrls {
    private static final String TEST_PAGE_URL = "http://www.mozilla.org";

    private static final String TEST_ICON_HTTP_URL = "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png";
    private static final String TEST_ICON_HTTP_URL_2 = "https://www.mozilla.org/media/img/favicon.52506929be4c.ico";
    private static final String TEST_ICON_JAR_URL = "jar:jar:wtf.png";

    @Test
    public void testFiltering() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_HTTP_URL))
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_HTTP_URL_2))
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_JAR_URL))
                .build();

        Assert.assertEquals(3, request.getIconCount());

        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL));
        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL_2));
        Assert.assertTrue(containsUrl(request, TEST_ICON_JAR_URL));

        Preparer preparer = new FilterPrivilegedUrls();
        preparer.prepare(request);

        Assert.assertEquals(2, request.getIconCount());

        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL));
        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL_2));
        Assert.assertFalse(containsUrl(request, TEST_ICON_JAR_URL));
    }

    @Test
    public void testNothingIsFilteredForPrivilegedRequests() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(TEST_PAGE_URL)
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_HTTP_URL))
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_HTTP_URL_2))
                .icon(IconDescriptor.createGenericIcon(TEST_ICON_JAR_URL))
                .privileged(true)
                .build();

        Assert.assertEquals(3, request.getIconCount());

        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL));
        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL_2));
        Assert.assertTrue(containsUrl(request, TEST_ICON_JAR_URL));

        Preparer preparer = new FilterPrivilegedUrls();
        preparer.prepare(request);

        Assert.assertEquals(3, request.getIconCount());

        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL));
        Assert.assertTrue(containsUrl(request, TEST_ICON_HTTP_URL_2));
        Assert.assertTrue(containsUrl(request, TEST_ICON_JAR_URL));
    }

    private boolean containsUrl(IconRequest request, String url) {
        final Iterator<IconDescriptor> iterator = request.getIconIterator();

        while (iterator.hasNext()) {
            IconDescriptor descriptor = iterator.next();

            if (descriptor.getUrl().equals(url)) {
                return true;
            }
        }

        return false;
    }
}
