/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.loader;

import android.annotation.TargetApi;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.helpers.MockUserManager;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconRequestBuilder;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(TestRunner.class)
public class TestSuggestedSiteLoader {

    private Context context;

    private static final String[] DEFAULT_SUGGESTED_SITES_ICONS = {
            "https://m.facebook.com/",
            "https://m.youtube.com/",
            "https://www.amazon.com/",
            "https://www.wikipedia.org/",
            "https://mobile.twitter.com/"
    };

    @Before
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    public void setUp() throws IOException {
        context = MockUserManager.getContextWithMockedUserManager();

        final SuggestedSites sites = new SuggestedSites(context);
        BrowserDB.from(context).setSuggestedSites(sites);

        // Force loading sites, so that SuggestedSites is correctly initialised. Without this, we
        // won't be able to retrieve site data (i.e. icon locations and colours).
        final Cursor sitesCursor = sites.get(200);
        sitesCursor.close();
    }

    @Test
    public void testLoadingNonSuggestedIcons() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon("http://www.mozilla.org/favicon.ico"))
                .build();

        final IconLoader loader = new SuggestedSiteLoader();
        final IconResponse response = loader.load(request);

        Assert.assertNull(response);
    }

    private IconResponse loadIconForSite(final String site, final boolean skipDisk) throws IOException {
        IconRequestBuilder requestBuilder = Icons.with(RuntimeEnvironment.application)
                .pageUrl(site)
                .icon(IconDescriptor.createGenericIcon(SuggestedSiteLoader.SUGGESTED_SITE_TOUCHTILE + site));

        if (skipDisk) {
            requestBuilder = requestBuilder.skipDisk();
        }

        final IconRequest request = requestBuilder.build();

        // We have to mock the loader: there's no practical reason for not loading the actual images,
        // but Picasso expects to run off the main thread, and RoboElectric only has one thread.
        final SuggestedSiteLoader loader = spy(new SuggestedSiteLoader());
        // Returning any valid bitmap is good enough:
        doReturn(
                Bitmap.createBitmap(new int[] { Color.RED }, 1, 1, null))
                .when(loader)
                .loadBitmap(any(Context.class), anyString());

        return loader.load(request);
    }

    @Test
    public void testLoadingSuggestedIcons() throws IOException {
        for (final String site : DEFAULT_SUGGESTED_SITES_ICONS) {
            final IconResponse response = loadIconForSite(site, false);

            Assert.assertNotNull(response);

            final Bitmap out = response.getBitmap();
            final int expectedSize = context.getResources().getDimensionPixelSize(R.dimen.favicon_bg);
            Assert.assertEquals(expectedSize, out.getWidth());
            Assert.assertEquals(expectedSize, out.getWidth());
        }
    }

    @Test
    public void testLoadingOtherIconFailsCleanly() throws IOException {
        // Ensure that we return null (instead of crashing or providing wrong data) for a fake
        // topsite.
        final IconResponse response = loadIconForSite("http://www.arbitrary-not-bunled-site.notreal", false);

        Assert.assertNull(response);
    }


    @Test
    public void testRespectsSkipDisk() throws IOException {
        for (final String site : DEFAULT_SUGGESTED_SITES_ICONS) {
            final IconResponse response = loadIconForSite(site, true);

            Assert.assertNull(response);
        }
    }
}
