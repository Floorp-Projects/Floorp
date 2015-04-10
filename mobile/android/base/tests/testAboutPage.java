/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;

import android.app.Activity;

/* Tests related to the about: page:
 *  - check that about: loads from the URL bar
 *  - check that about: loads from Settings/About...
 */
public class testAboutPage extends PixelTest {

    public void testAboutPage() {
        blockForGeckoReady();

        // Load the about: page and verify its title.
        String url = mStringHelper.ABOUT_SCHEME;
        loadAndPaint(url);

        verifyUrlBarTitle(url);

        // Open a new page to remove the about: page from the current tab.
        url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        loadUrlAndWait(url);

        // At this point the page title should have been set.
        verifyUrlBarTitle(url);

        // Set up listeners to catch the page load we're about to do.
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        selectSettingsItem(mStringHelper.MOZILLA_SECTION_LABEL, mStringHelper.ABOUT_LABEL);

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Make sure the about: page was loaded.
        verifyUrlBarTitle(mStringHelper.ABOUT_SCHEME);
    }
}
