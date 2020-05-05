/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.interactor

import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify

class DefaultInteractorTest {

    private val accountManager: FxaAccountManager = mock()
    private val view: SyncedTabsView = mock()

    @Test
    fun start() = runBlockingTest {
        val view =
            TestSyncedTabsView()
        val feature = DefaultInteractor(
            accountManager,
            view,
            coroutineContext
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
            accountManager,
            view,
            coroutineContext
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
            accountManager,
            view,
            coroutineContext
        ) {
            invoked = true
        }

        feature.onTabClicked(mock())

        assertTrue(invoked)
    }

    @Test
    fun `onRefresh does not update devices when there is no constellation`() = runBlockingTest {
        val feature = DefaultInteractor(
            accountManager,
            view,
            coroutineContext
        ) {}

        feature.onRefresh()

        verify(accountManager).syncNowAsync(SyncReason.User, true)
    }

    @Test
    fun `onRefresh updates devices when there is a constellation`() = runBlockingTest {
        val feature = DefaultInteractor(
            accountManager,
            view,
            coroutineContext
        ) {}
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(accountManager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        feature.onRefresh()

        verify(constellation).refreshDevicesAsync()
        verify(accountManager).syncNowAsync(SyncReason.User, true)
    }

    private class TestSyncedTabsView : SyncedTabsView {
        override var listener: SyncedTabsView.Listener? = null

        override fun onError(error: SyncedTabsView.ErrorType) {
        }

        override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        }
    }
}
