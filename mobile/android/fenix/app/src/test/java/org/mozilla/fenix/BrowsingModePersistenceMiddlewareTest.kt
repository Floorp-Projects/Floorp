/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.verify
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.utils.Settings

class BrowsingModePersistenceMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = mock()
    }

    @Test
    fun `WHEN store initialization intercepted THEN mode read from settings and update dispatched`() = runTest {
        val cachedMode = BrowsingMode.Private
        whenever(settings.lastKnownMode).thenReturn(cachedMode)

        val middleware = BrowsingModePersistenceMiddleware(settings, this)
        val store = AppStore(middlewares = listOf(middleware))
        // Wait for Init action
        store.waitUntilIdle()
        // Wait for middleware launched coroutine
        this.advanceUntilIdle()
        // Wait for ModeChange action
        store.waitUntilIdle()

        assertEquals(cachedMode, store.state.mode)
    }

    @Test
    fun `WHEN mode change THEN mode updated on disk`() = runTest {
        val cachedMode = BrowsingMode.Normal
        whenever(settings.lastKnownMode).thenReturn(cachedMode)
        val updatedMode = BrowsingMode.Private

        val middleware = BrowsingModePersistenceMiddleware(settings, this)
        val store = AppStore(middlewares = listOf(middleware))
        store.dispatch(AppAction.ModeChange(updatedMode)).joinBlocking()
        this.advanceUntilIdle()

        verify(settings).lastKnownMode = updatedMode
    }
}
