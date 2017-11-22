/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
package org.mozilla.gecko.icons.preparation;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestAboutPagesPreparer {
    private static final String[] ABOUT_PAGES = {
            AboutPages.ACCOUNTS,
            AboutPages.ADDONS,
            AboutPages.CONFIG,
            AboutPages.DOWNLOADS,
            AboutPages.FIREFOX,
            AboutPages.HOME
    };

    @Test
    public void testPreparerAddsUrlsForAllAboutPages() {
        final Preparer preparer = new AboutPagesPreparer();

        for (String url : ABOUT_PAGES) {
            final IconRequest request = Icons.with(RuntimeEnvironment.application)
                    .pageUrl(url)
                    .build();

            Assert.assertEquals(0, request.getIconCount());

            preparer.prepare(request);

            Assert.assertEquals("Added icon URL for URL: " + url, 1, request.getIconCount());
        }
    }

    @Test
    public void testPrepareDoesNotAddUrlForGenericHttpUrl() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .build();

        Assert.assertEquals(0, request.getIconCount());

        final Preparer preparer = new AboutPagesPreparer();
        preparer.prepare(request);

        Assert.assertEquals(0, request.getIconCount());
    }

    @Test
    public void testAddedUrlHasJarScheme() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl(AboutPages.DOWNLOADS)
                .build();

        final Preparer preparer = new AboutPagesPreparer();
        preparer.prepare(request);

        Assert.assertEquals(1, request.getIconCount());

        final String url = request.getBestIcon().getUrl();
        Assert.assertNotNull(url);
        Assert.assertTrue(url.startsWith("jar:jar:"));
    }
}
