/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import com.google.android.material.tabs.TabLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class TabLayoutObserverTest {
    private val interactor = mockk<TabsTrayInteractor>(relaxed = true)
    private lateinit var store: TabsTrayStore
    private val middleware = CaptureActionsMiddleware<TabsTrayState, TabsTrayAction>()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        store = TabsTrayStore(middlewares = listOf(middleware))
    }

    @Test
    fun `WHEN tab is selected THEN notify the interactor`() {
        val observer = TabLayoutObserver(interactor)
        val tab = mockk<TabLayout.Tab>()
        every { tab.position } returns 1
        assertNull(TabsTray.privateModeTapped.testGetValue())

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(1, false) }
        assertNotNull(TabsTray.privateModeTapped.testGetValue())

        every { tab.position } returns 0
        assertNull(TabsTray.normalModeTapped.testGetValue())

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(0, true) }
        assertNotNull(TabsTray.normalModeTapped.testGetValue())

        every { tab.position } returns 2
        assertNull(TabsTray.syncedModeTapped.testGetValue())

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(2, true) }
        assertNotNull(TabsTray.syncedModeTapped.testGetValue())
    }

    @Test
    fun `WHEN observer is first started THEN do not smooth scroll`() {
        val observer = TabLayoutObserver(interactor)
        val tab = mockk<TabLayout.Tab>()
        every { tab.position } returns 1
        assertNull(TabsTray.privateModeTapped.testGetValue())

        observer.onTabSelected(tab)

        verify { interactor.onTrayPositionSelected(1, false) }
        assertNotNull(TabsTray.privateModeTapped.testGetValue())

        observer.onTabSelected(tab)

        verify { interactor.onTrayPositionSelected(1, true) }
        assertNotNull(TabsTray.privateModeTapped.testGetValue())
    }
}
