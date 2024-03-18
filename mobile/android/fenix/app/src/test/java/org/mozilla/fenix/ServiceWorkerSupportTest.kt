/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.components

class ServiceWorkerSupportTest {
    private lateinit var activity: HomeActivity
    private lateinit var feature: ServiceWorkerSupportFeature
    private lateinit var engine: GeckoEngine
    private lateinit var addNewTabUseCase: AddNewTabUseCase

    @Before
    fun setup() {
        activity = mockk()
        engine = mockk(relaxed = true)
        feature = ServiceWorkerSupportFeature(activity)
        addNewTabUseCase = mockk(relaxed = true)
        every { activity.components.core.engine } returns engine
        every { activity.components.useCases.tabsUseCases.addTab } returns addNewTabUseCase
        every { activity.openToBrowser(BrowserDirection.FromHome) } just Runs
    }

    @Test
    fun `GIVEN the feature is registered for lifecycle events WHEN the owner is created THEN register itself as a service worker delegate`() {
        feature.onCreate(mockk())

        verify { engine.registerServiceWorkerDelegate(feature) }
    }

    @Test
    fun `GIVEN the feature is registered for lifecycle events WHEN the owner is destroyed THEN unregister itself as a service worker delegate`() {
        feature.onDestroy(mockk())

        verify { engine.unregisterServiceWorkerDelegate() }
    }

    @Test
    fun `WHEN a new tab is requested THEN navigate to browser then add a new tab`() {
        val engineSession: EngineSession = mockk()
        feature.addNewTab(engineSession)

        verifyOrder {
            activity.openToBrowser(BrowserDirection.FromHome)

            addNewTabUseCase(
                url = "about:blank",
                selectTab = true,
                startLoading = true,
                parentId = null,
                flags = LoadUrlFlags.external(),
                contextId = null,
                engineSession = engineSession,
                source = SessionState.Source.Internal.None,
                searchTerms = "",
                private = false,
                historyMetadata = null,
                isSearch = false,
            )
        }
    }

    @Test
    fun `WHEN a new tab is requested THEN return true`() {
        val result = feature.addNewTab(mockk())

        assertTrue(result)
    }
}
