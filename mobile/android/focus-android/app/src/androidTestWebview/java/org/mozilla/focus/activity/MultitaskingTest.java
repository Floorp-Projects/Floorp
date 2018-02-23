/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.os.SystemClock;
import android.support.test.espresso.web.webdriver.Locator;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.web.IWebView;

import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static android.support.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static android.support.test.espresso.web.sugar.Web.onWebView;
import static android.support.test.espresso.web.webdriver.DriverAtoms.findElement;
import static android.support.test.espresso.web.webdriver.DriverAtoms.getText;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.mozilla.focus.helpers.EspressoHelper.navigateToMockWebServer;
import static org.mozilla.focus.helpers.EspressoHelper.onFloatingEraseButton;
import static org.mozilla.focus.helpers.EspressoHelper.onFloatingTabsButton;
import static org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset;
import static org.mozilla.focus.helpers.WebViewFakeLongPress.injectHitTarget;

/**
 * Open multiple sessions and verify that the UI looks like it should.
 */
@RunWith(AndroidJUnit4.class)
public class MultitaskingTest {
    @Rule
    public MainActivityFirstrunTestRule mActivityTestRule = new MainActivityFirstrunTestRule(false);

    private MockWebServer webServer;

    @Before
    public void startWebServer() throws Exception {
        webServer = new MockWebServer();

        webServer.enqueue(createMockResponseFromAsset("tab1.html"));
        webServer.enqueue(createMockResponseFromAsset("tab2.html"));
        webServer.enqueue(createMockResponseFromAsset("tab3.html"));

        webServer.enqueue(createMockResponseFromAsset("tab2.html"));

        webServer.start();
    }

    @After
    public void stopWebServer() throws Exception {
        webServer.shutdown();
    }

    @Test
    public void testVisitingMultipleSites() {
        {
            // Load website: Erase button visible, Tabs button not

            navigateToMockWebServer(webServer, "tab1.html");

            checkTabIsLoaded("Tab 1");

            onFloatingEraseButton()
                    .check(matches(isDisplayed()));

            onFloatingTabsButton()
                    .check(matches(not(isDisplayed())));
        }

        {
            // Open link in new tab: Erase button hidden, Tabs button visible

            longPressLink("tab2", "Tab 2", "tab2.html");

            openInNewTab();

            onFloatingEraseButton()
                    .check(matches(not(isDisplayed())));

            onFloatingTabsButton()
                    .check(matches(isDisplayed()))
                    .check(matches(withContentDescription(is("Tabs open: 2"))));
        }

        {
            // Open link in new tab: Tabs button updated, Erase button still hidden

            longPressLink("tab3", "Tab 3", "tab3.html");

            openInNewTab();

            onFloatingEraseButton()
                    .check(matches(not(isDisplayed())));

            onFloatingTabsButton()
                    .check(matches(isDisplayed()))
                    .check(matches(withContentDescription(is("Tabs open: 3"))));
        }

        {
            // Open tabs tray and switch to second tab.

            onFloatingTabsButton()
                    .perform(click());

            final String expectedUrl = webServer.getHostName() + "/tab2.html";

            onView(withText(expectedUrl))
                    .perform(click());

            onWebView()
                    .withElement(findElement(Locator.ID, "content"))
                    .check(webMatches(getText(), equalTo("Tab 2")));
        }

        {
            // Remove all tabs via the menu

            onFloatingTabsButton()
                    .perform(click());

            onView(withText(R.string.tabs_tray_action_erase))
                    .perform(click());

            assertFalse(SessionManager.getInstance().hasSession());
        }


        SystemClock.sleep(5000);
    }

    private void checkTabIsLoaded(String title) {
        onWebView()
                .withElement(findElement(Locator.ID, "content"))
                .check(webMatches(getText(), equalTo(title)));
    }

    private void longPressLink(String id, String label, String path) {
        onWebView()
                .withElement(findElement(Locator.ID, id))
                .check(webMatches(getText(), equalTo(label)));

        simulateLinkLongPress(path);
    }

    // Webview only method
    private void simulateLinkLongPress(String path) {
        onView(withId(R.id.webview))
                .perform(injectHitTarget(
                        new IWebView.HitTarget(true, webServer.url(path).toString(), false, null)));
    }

    private void openInNewTab() {
        onView(withText(R.string.contextmenu_open_in_new_tab))
                .perform(click());
    }
}
