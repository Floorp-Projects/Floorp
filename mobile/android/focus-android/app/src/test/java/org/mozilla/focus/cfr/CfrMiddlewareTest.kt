/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cfr

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.MockitoAnnotations
import org.mozilla.focus.ext.components
import org.mozilla.focus.nimbus.FocusNimbus
import org.mozilla.focus.nimbus.Onboarding
import org.mozilla.focus.state.AppStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CfrMiddlewareTest {
    private lateinit var onboardingExperiment: Onboarding
    private val browserStore: BrowserStore = testContext.components.store
    private val appStore: AppStore = testContext.components.appStore

    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
        onboardingExperiment = FocusNimbus.features.onboarding.value(testContext)
    }

    @Test
    fun `GIVEN erase cfr is enabled and tracking protection cfr is not displayed WHEN AddTabAction is intercepted THEN the numberOfTabsOpened is increased`() {
        if (onboardingExperiment.isCfrEnabled) {
            browserStore.dispatch(TabListAction.AddTabAction(createTab())).joinBlocking()

            assertEquals(1, testContext.components.settings.numberOfTabsOpened)
        }
    }

    @Test
    fun `GIVEN erase cfr is enabled and tracking protection cfr is not displayed WHEN AddTabAction is intercepted for the third time THEN showEraseTabsCfr is changed to true`() {
        if (onboardingExperiment.isCfrEnabled) {
            browserStore.dispatch(TabListAction.AddTabAction(createTab(tabId = 1))).joinBlocking()
            browserStore.dispatch(TabListAction.AddTabAction(createTab(tabId = 2))).joinBlocking()
            browserStore.dispatch(TabListAction.AddTabAction(createTab(tabId = 3))).joinBlocking()
            appStore.waitUntilIdle()

            assertTrue(appStore.state.showEraseTabsCfr)
        }
    }

    @Test
    fun `GIVEN shouldShowCfrForTrackingProtection is true WHEN UpdateSecurityInfoAction is intercepted THEN showTrackingProtectionCfr is changed to true`() {
        if (onboardingExperiment.isCfrEnabled) {
            val updateSecurityInfoAction = ContentAction.UpdateSecurityInfoAction(
                "1",
                SecurityInfoState(
                    secure = true,
                    host = "test.org",
                    issuer = "Test"
                )
            )
            val trackerBlockedAction = TrackingProtectionAction.TrackerBlockedAction(
                tabId = "1",
                tracker = Tracker(
                    url = "test.org",
                    trackingCategories = listOf(EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING),
                    cookiePolicies = listOf(EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE)
                )
            )

            browserStore.dispatch(updateSecurityInfoAction).joinBlocking()
            browserStore.dispatch(trackerBlockedAction).joinBlocking()
            appStore.waitUntilIdle()

            assertTrue(appStore.state.showTrackingProtectionCfrForTab.getOrDefault("1", false))
        }
    }

    @Test
    fun `GIVEN insecure tab WHEN UpdateSecurityInfoAction is intercepted THEN showTrackingProtectionCfr is not changed to true`() {
        if (onboardingExperiment.isCfrEnabled) {
            val insecureTab = createTab(isSecure = false)
            val updateSecurityInfoAction = ContentAction.UpdateSecurityInfoAction(
                "1",
                SecurityInfoState(
                    secure = false,
                    host = "test.org",
                    issuer = "Test"
                )
            )

            browserStore.dispatch(TabListAction.AddTabAction(insecureTab)).joinBlocking()
            browserStore.dispatch(updateSecurityInfoAction).joinBlocking()
            appStore.waitUntilIdle()

            assertFalse(appStore.state.showTrackingProtectionCfrForTab.getOrDefault("1", false))
        }
    }

    @Test
    fun `GIVEN mozilla tab WHEN UpdateSecurityInfoAction is intercepted THEN showTrackingProtectionCfr is not changed to true`() {
        if (onboardingExperiment.isCfrEnabled) {
            val mozillaTab = createTab(id = "1", url = "https://www.mozilla.org")
            val updateSecurityInfoAction = ContentAction.UpdateSecurityInfoAction(
                "1",
                SecurityInfoState(
                    secure = true,
                    host = "test.org",
                    issuer = "Test"
                )
            )
            browserStore.dispatch(TabListAction.AddTabAction(mozillaTab)).joinBlocking()
            browserStore.dispatch(updateSecurityInfoAction).joinBlocking()
            appStore.waitUntilIdle()

            assertFalse(appStore.state.showTrackingProtectionCfrForTab.getOrDefault("1", false))
        }
    }

    private fun createTab(
        tabUrl: String = "https://www.test.org",
        tabId: Int = 1,
        isSecure: Boolean = true
    ): TabSessionState {
        val tab = createTab(tabUrl, id = tabId.toString())
        return tab.copy(
            content = tab.content.copy(
                private = true,
                securityInfo = SecurityInfoState(secure = isSecure)
            )
        )
    }
}
