/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.web.assertion.WebViewAssertions
import androidx.test.espresso.web.sugar.Web
import androidx.test.espresso.web.webdriver.DriverAtoms
import androidx.test.espresso.web.webdriver.Locator
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import junit.framework.TestCase
import mozilla.components.browser.state.selector.privateTabs
import okhttp3.mockwebserver.MockWebServer
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.helpers.EspressoHelper.navigateToMockWebServer
import org.mozilla.focus.helpers.EspressoHelper.onFloatingEraseButton
import org.mozilla.focus.helpers.EspressoHelper.onFloatingTabsButton
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.SessionLoadedIdlingResource
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset

/**
 * Open multiple sessions and verify that the UI looks like it should.
 */
@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class MultitaskingTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)
    private var webServer: MockWebServer? = null
    private var loadingIdlingResource: SessionLoadedIdlingResource? = null

    @Before
    @Throws(Exception::class)
    fun startWebServer() {
        webServer = MockWebServer()
        webServer!!.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer!!.enqueue(createMockResponseFromAsset("tab2.html"))
        webServer!!.enqueue(createMockResponseFromAsset("tab3.html"))
        webServer!!.enqueue(createMockResponseFromAsset("tab2.html"))
        webServer!!.start()
        loadingIdlingResource = SessionLoadedIdlingResource()
        IdlingRegistry.getInstance().register(loadingIdlingResource)
    }

    @After
    @Throws(Exception::class)
    fun stopWebServer() {
        IdlingRegistry.getInstance().unregister(loadingIdlingResource)
        mActivityTestRule.activity.finishAndRemoveTask()
        webServer!!.shutdown()
    }

    @Test
    fun testVisitingMultipleSites() {
        run {

            // Load website: Erase button visible, Tabs button not
            TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime)
            navigateToMockWebServer(webServer!!, "tab1.html")
            checkTabIsLoaded("Tab 1")
            onFloatingEraseButton()
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            onFloatingTabsButton()
                .check(
                    ViewAssertions.matches(
                        Matchers.not(
                            ViewMatchers.isDisplayed()
                        )
                    )
                )
        }
        run {

            // Open link in new tab: Erase button hidden, Tabs button visible
            longPressLink("tab2", "Tab 2")
            openInNewTab()

            // verify Tab 1 is still on foreground
            checkTabIsLoaded("Tab 1")
            onFloatingEraseButton()
                .check(
                    ViewAssertions.matches(
                        Matchers.not(
                            ViewMatchers.isDisplayed()
                        )
                    )
                )
            onFloatingTabsButton()
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
                .check(
                    ViewAssertions.matches(
                        ViewMatchers.withContentDescription(
                            Matchers.`is`(
                                "Tabs open: 2"
                            )
                        )
                    )
                )
        }
        run {

            // Open link in new tab: Tabs button updated, Erase button still hidden
            longPressLink("tab3", "Tab 3")
            openInNewTab()

            // verify Tab 1 is still on foreground
            checkTabIsLoaded("Tab 1")
            onFloatingEraseButton()
                .check(
                    ViewAssertions.matches(
                        Matchers.not(
                            ViewMatchers.isDisplayed()
                        )
                    )
                )
            onFloatingTabsButton()
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
                .check(
                    ViewAssertions.matches(
                        ViewMatchers.withContentDescription(
                            Matchers.`is`(
                                "Tabs open: 3"
                            )
                        )
                    )
                )
        }
        run {

            // Open tabs tray and switch to second tab.
            onFloatingTabsButton()
                .perform(ViewActions.click())

            // Tab title would not have the port number, since the site isn't visited yet
            Espresso.onView(ViewMatchers.withText(webServer!!.hostName + "/tab2.html"))
                .perform(ViewActions.click())
            checkTabIsLoaded("Tab 2")
        }
        run {

            // Remove all tabs via the menu
            onFloatingTabsButton()
                .perform(ViewActions.click())
            Espresso.onView(withText(R.string.tabs_tray_action_erase))
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
                .perform(ViewActions.click())

            // Now on main view
            TestCase.assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime))
            val store = InstrumentationRegistry.getInstrumentation().getTargetContext()
                .applicationContext.components
                .store
            TestCase.assertTrue(store.state.privateTabs.isEmpty())
        }
    }

    private fun checkTabIsLoaded(title: String) {
        Web.onWebView()
            .withElement(DriverAtoms.findElement(Locator.ID, "content"))
            .check(WebViewAssertions.webMatches(DriverAtoms.getText(), Matchers.equalTo(title)))
    }

    private fun longPressLink(id: String, label: String) {
        Web.onWebView()
            .withElement(DriverAtoms.findElement(Locator.ID, id))
            .check(WebViewAssertions.webMatches(DriverAtoms.getText(), Matchers.equalTo(label)))

        // Removed the following code since it depended on the IWebView abstraction that was removed
        // in favor of the AC engine components.
        // simulateLinkLongPress(path);
    }

    private fun openInNewTab() {
        Espresso.onView(withText(R.string.contextmenu_open_in_new_tab))
            .perform(ViewActions.click())
        checkNewTabPopup()
    }

    private fun checkNewTabPopup() {
        TestHelper.mDevice.wait(
            Until.findObject(
                By.res(appName, "snackbar_text")
            ), 5000
        )
        TestHelper.mDevice.wait(
            Until.gone(
                By.res(appName, "snackbar_text")
            ), 5000
        )
    }
}
