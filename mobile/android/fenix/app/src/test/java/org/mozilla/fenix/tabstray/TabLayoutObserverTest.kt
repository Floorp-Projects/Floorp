/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import com.google.android.material.tabs.TabLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Before
import org.junit.Test

class TabLayoutObserverTest {
    private val interactor = mockk<TabsTrayInteractor>(relaxed = true)
    private lateinit var store: TabsTrayStore
    private val middleware = CaptureActionsMiddleware<TabsTrayState, TabsTrayAction>()

    @Before
    fun setup() {
        store = TabsTrayStore(middlewares = listOf(middleware))
    }

    @Test
    fun `WHEN tab is selected THEN notify the interactor`() {
        val observer = TabLayoutObserver(interactor)
        val tab = mockk<TabLayout.Tab>()
        every { tab.position } returns 1

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(1, false) }

        every { tab.position } returns 0

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(0, true) }

        every { tab.position } returns 2

        observer.onTabSelected(tab)

        store.waitUntilIdle()

        verify { interactor.onTrayPositionSelected(2, true) }
    }

    @Test
    fun `WHEN observer is first started THEN do not smooth scroll`() {
        val observer = TabLayoutObserver(interactor)
        val tab = mockk<TabLayout.Tab>()
        every { tab.position } returns 1

        observer.onTabSelected(tab)

        verify { interactor.onTrayPositionSelected(1, false) }

        observer.onTabSelected(tab)

        verify { interactor.onTrayPositionSelected(1, true) }
    }
}
