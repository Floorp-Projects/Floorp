/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
public class TestAddDefaultIconUrl {
    @Test
    public void testAddingDefaultUrl() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createTouchicon(
                        "https://www.mozilla.org/media/img/favicon/apple-touch-icon-180x180.00050c5b754e.png",
                        180,
                        "image/png"))
                .icon(IconDescriptor.createFavicon(
                        "https://www.mozilla.org/media/img/favicon.52506929be4c.ico",
                        32,
                        "image/x-icon"))
                .icon(IconDescriptor.createFavicon(
                        "jar:jar:wtf.png",
                        16,
                        "image/png"))
                .build();


        Assert.assertEquals(3, request.getIconCount());
        Assert.assertFalse(containsUrl(request, "http://www.mozilla.org/favicon.ico"));

        Preparer preparer = new AddDefaultIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(4, request.getIconCount());
        Assert.assertTrue(containsUrl(request, "http://www.mozilla.org/favicon.ico"));
    }

    @Test
    public void testDefaultUrlIsNotAddedIfItAlreadyExists() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createFavicon(
                        "http://www.mozilla.org/favicon.ico",
                        32,
                        "image/x-icon"))
                .build();

        Assert.assertEquals(1, request.getIconCount());

        Preparer preparer = new AddDefaultIconUrl();
        preparer.prepare(request);

        Assert.assertEquals(1, request.getIconCount());
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
