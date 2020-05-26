/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.interactor

import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verify

class DefaultInteractorTest {

    private val view: SyncedTabsView = mock()
    private val controller: SyncedTabsController = mock()

    @Test
    fun start() = runBlockingTest {
        val view =
            TestSyncedTabsView()
        val feature = DefaultInteractor(
            controller,
            view
        ) {}

        assertNull(view.listener)

        feature.start()

        assertNotNull(view.listener)
    }

    @Test
    fun stop() = runBlockingTest {
        val view =
            TestSyncedTabsView()
        val feature = DefaultInteractor(
            controller,
            view
        ) {}

        assertNull(view.listener)

        feature.start()

        assertNotNull(view.listener)

        feature.stop()

        assertNull(view.listener)
    }

    @Test
    fun `onTabClicked invokes callback`() = runBlockingTest {
        var invoked = false
        val feature = DefaultInteractor(
            controller,
            view
        ) {
            invoked = true
        }

        feature.onTabClicked(mock())

        assertTrue(invoked)
    }

    @Test
    fun `onRefresh does not update devices when there is no constellation`() = runBlockingTest {
        val feature = DefaultInteractor(
            controller,
            view
        ) {}

        feature.onRefresh()

        verify(controller).syncAccount()
    }

    @Test
    fun `onRefresh updates devices when there is a constellation`() = runBlockingTest {
        val feature = DefaultInteractor(
            controller,
            view
        ) {}

        feature.onRefresh()

        verify(controller).syncAccount()
    }

    private class TestSyncedTabsView : SyncedTabsView {
        override var listener: SyncedTabsView.Listener? = null

        override fun onError(error: SyncedTabsView.ErrorType) {
        }

        override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        }
    }
}
