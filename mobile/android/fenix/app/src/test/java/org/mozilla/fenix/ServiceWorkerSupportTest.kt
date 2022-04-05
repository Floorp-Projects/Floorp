/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.state.state.SessionState.Source.Internal.None
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.test.eq
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection.FromHome
import org.mozilla.fenix.ext.components

@RunWith(AndroidJUnit4::class)
class ServiceWorkerSupportTest {
    @Before
    fun setup() {
        // Needed to mock the response of the "Context.components" extension property.
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
    }

    @After
    fun teardown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
    }

    @Test
    fun `GIVEN the feature is registered for lifecycle events WHEN the owner is created THEN register itself as a service worker delegate`() {
        val engine: GeckoEngine = mockk(relaxed = true)
        every { any<Context>().components.core.engine } returns engine
        val feature = ServiceWorkerSupportFeature(mockk(relaxed = true))

        feature.onCreate(mockk())

        verify { engine.registerServiceWorkerDelegate(feature) }
    }

    @Test
    fun `GIVEN the feature is registered for lifecycle events WHEN the owner is destroyed THEN unregister itself as a service worker delegate`() {
        val engine: GeckoEngine = mockk(relaxed = true)
        every { any<Context>().components.core.engine } returns engine
        val feature = ServiceWorkerSupportFeature(mockk(relaxed = true))

        feature.onDestroy(mockk())

        verify { engine.unregisterServiceWorkerDelegate() }
    }

    @Test
    fun `WHEN a new tab is requested THEN navigate to browser then add a new tab`() {
        val addNewTabUseCase: AddNewTabUseCase = mockk(relaxed = true)
        every { any<Context>().components.useCases.tabsUseCases.addTab } returns addNewTabUseCase
        val activity: HomeActivity = mockk(relaxed = true)
        val feature = ServiceWorkerSupportFeature(activity)

        feature.addNewTab(mockk())

        verifyOrder {
            activity.openToBrowser(FromHome)

            addNewTabUseCase(
                url = eq("about:blank"),
                selectTab = eq(true), // default
                startLoading = eq(true), // default
                parentId = eq(null), // default
                flags = eq(LoadUrlFlags.external()),
                contextId = eq(null), // default
                engineSession = any(),
                source = eq(None),
                searchTerms = eq(""), // default
                private = eq(false), // default
                historyMetadata = eq(null) // default
            )
        }
    }

    @Test
    fun `WHEN a new tab is requested THEN return true`() {
        val addNewTabUseCase: AddNewTabUseCase = mockk(relaxed = true)
        every { any<Context>().components.useCases.tabsUseCases.addTab } returns addNewTabUseCase

        val activity: HomeActivity = mockk(relaxed = true)
        val feature = ServiceWorkerSupportFeature(activity)

        val result = feature.addNewTab(mockk())

        assertTrue(result)
    }
}
