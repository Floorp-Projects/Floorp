/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.preparation;

import android.content.Context;
import android.database.Cursor;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.helpers.MockUserManager;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.icons.loader.SuggestedSiteLoader;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;

import static junit.framework.Assert.assertEquals;

@RunWith(TestRunner.class)
public class TestSuggestedSitePreparer {
    private static final String[] DEFAULT_SUGGESTED_SITES_ICONS = {
            "https://m.facebook.com/",
            "https://m.youtube.com/",
            "https://www.amazon.com/",
            "https://www.wikipedia.org/",
            "https://mobile.twitter.com/"

    };

    private Context context;

    @Before
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
    public void testNonSuggestedURL() {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .build();

        Assert.assertEquals(0, request.getIconCount());

        final Preparer preparer = new SuggestedSitePreparer();
        preparer.prepare(request);

        Assert.assertEquals(0, request.getIconCount());
    }

    @Test
    public void testSuggestedIconsAdded() {
        for (String site : DEFAULT_SUGGESTED_SITES_ICONS) {

            final IconRequest request = Icons.with(RuntimeEnvironment.application)
                    .pageUrl(site)
                    .build();

            Assert.assertEquals(0, request.getIconCount());

            final Preparer preparer = new SuggestedSitePreparer();
            preparer.prepare(request);

            Assert.assertEquals(1, request.getIconCount());

            // We use the site URL as the icon URL (along with the touchtile "protocol"),
            // SuggestSiteLoader then translates that to the appropriate underlying icons:
            assertEquals(SuggestedSiteLoader.SUGGESTED_SITE_TOUCHTILE + site,
                    request.getBestIcon().getUrl());

        }
    }
}
