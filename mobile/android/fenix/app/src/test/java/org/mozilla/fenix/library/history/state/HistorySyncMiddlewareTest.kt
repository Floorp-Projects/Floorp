/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.verify
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

class HistorySyncMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN sync is started THEN account manager handles sync, the view is refreshed, and the sync is finished`() = runTestOnMain {
        var viewIsRefreshed = false
        val accountManager = mock<FxaAccountManager>()
        val middleware = HistorySyncMiddleware(
            accountManager = accountManager,
            refreshView = { viewIsRefreshed = true },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.StartSync).joinBlocking()
        store.waitUntilIdle()

        assertTrue(viewIsRefreshed)
        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
        verify(accountManager).syncNow(
            reason = SyncReason.User,
            debounce = true,
            customEngineSubset = listOf(SyncEngine.History),
        )
    }
}
