/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;
import android.util.Log;
import com.robotium.solo.Condition;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.homepanel.topstories.PocketStoriesLoader;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;
import org.mozilla.gecko.util.StringUtils;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

/**
 * It's very important that suggestions from Pocket are opened with a Pocket URI in the referrer: this is
 * what this test is for.
 *
 * We want to verify:
 * - Clicking or long clicking a Pocket top story has a Pocket referrer.
 * - Clicking or long clicking other items on top sites does not have a Pocket referrer.
 *
 * The ideal test is to set up a server that will assert that clicks that should have a Pocket referrer
 * have one in the request headers and clicks that should not have a referrer do not have one. However,
 * it's non-trivial to set up such a server in our harness so we instead intercept Tab:Load JS events
 * and verify they have the referrer. This isn't ideal because we might drop the ball passing the referrer
 * to Gecko, or Gecko might drop the ball, but this test should help regressions if we manually test the
 * referrers are working.
 */
public class testActivityStreamPocketReferrer extends JavascriptBridgeTest {

    private static final String LOGTAG =
            StringUtils.safeSubstring(testActivityStreamPocketReferrer.class.getSimpleName(), 0, 23);

    private static final String JS_FILE = "testActivityStreamPocketReferrer.js";

    private boolean wasTabLoadReceived = false;
    private boolean tabLoadContainsPocketReferrer = false;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Override the default placeholder URL so we don't access the network during testing.
        // Note: this actually only seems to take effect after we load a page and go back to about:home.
        PocketStoriesLoader.configureForTesting(getAbsoluteHostnameUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL));
    }

    public void testActivityStreamPocketReferrer() throws Exception {
        if (!getActivity().getResources().getBoolean(R.bool.pref_activitystream_pocket_enabled_default)) {
            // I don't have time to add code to force enable Pocket suggestions so we'll just assume
            // this won't break until we re-enable Pocket by default (bug 1411657).
            Log.d(LOGTAG, "Pocket recommendations are disabled by default; returning success...");
            return;
        }

        blockForReadyAndLoadJS(JS_FILE);
        NavigationHelper.goBack(); // to top sites.

        checkReferrerInTopStories();
        checkReferrerInTopStoriesContextMenu();

        checkNoReferrerInTopSites(); // relies on changes to Top Sites from previous tests.

        // Ideally, we'd also test that there is no referrer for highlights but it's more
        // challenging to get an item to show up in highlights (bookmark the page) and to scroll
        // to open it: to save time, I chose not to implement it.
    }

    private void checkReferrerInTopStories() {
        Log.d(LOGTAG, "testReferrerInTopStories");

        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                mSolo.clickOnText(PocketStoriesLoader.PLACEHOLDER_TITLE); // Click Top Story placeholder item.
            }
        });

        assertTabLoadEventContainsPocketReferrer(true);
        NavigationHelper.goBack(); // to top sites.
    }

    private void checkReferrerInTopStoriesContextMenu() throws Exception {
        Log.d(LOGTAG, "testReferrerInTopStoriesContextMenu");

        mSolo.clickLongOnText(PocketStoriesLoader.PLACEHOLDER_TITLE); // Open Top Story context menu.
        mSolo.clickOnText(mStringHelper.CONTEXT_MENU_OPEN_IN_NEW_TAB);
        WaitHelper.waitFor("context menu to close after item selection.", new Condition() {
            @Override
            public boolean isSatisfied() {
                return !mSolo.searchText(mStringHelper.CONTEXT_MENU_OPEN_IN_NEW_TAB);
            }
        }, 5000);

        // There's no simple way to block until a background page loads so instead, we sleep for 500ms.
        // Our JS listener is attached the whole time so if the message is sent, we'll receive it and cache it
        // while we're sleeping.
        Thread.sleep(500);
        assertTabLoadEventContainsPocketReferrer(true);
    }

    private void checkNoReferrerInTopSites() {
        Log.d(LOGTAG, "testNoReferrerInTopSites");

        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                // Through the previous tests, we've added a top site called "Browser Blank Page...".
                // Only part of that label will be visible, however.
                mSolo.clickOnText("Browser Bl"); // Click on a Top Site.
            }
        });

        assertTabLoadEventContainsPocketReferrer(false);
        NavigationHelper.goBack(); // to top sites.
    }

    private void assertTabLoadEventContainsPocketReferrer(final boolean expectedContainsReferrer) {
        // We intercept the Tab:Load event in JS and, due to limitations in JavascriptBridge,
        // store the data there until Java asks for it.
        getJS().syncCall("copyTabLoadEventMetadataToJava"); // expected to call copyTabLoad...Receiver

        fAssertTrue("Expected Tab:Load to be called", wasTabLoadReceived);
        fAssertEquals("Checking for expected existence of pocket referrer from Tab:Load event in JS",
                expectedContainsReferrer, tabLoadContainsPocketReferrer);
    }

    // JS methods.
    public void copyTabLoadEventMetadataToJavaReceiver(final boolean wasTabLoadReceived, final boolean tabLoadContainsPocketReferrer) {
        Log.d(LOGTAG, "setTabLoadContainsPocketReferrer called via JS: " + wasTabLoadReceived + ", " + tabLoadContainsPocketReferrer);
        this.wasTabLoadReceived = wasTabLoadReceived;
        this.tabLoadContainsPocketReferrer = tabLoadContainsPocketReferrer;
    }

    public void log(final String s) {
        Log.d(LOGTAG, "jsLog: " + s);
    }
}
