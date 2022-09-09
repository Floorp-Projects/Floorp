/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.content.Context
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.FenixApplication

class SyncedTabsIntegrationTest {

    @MockK private lateinit var context: Context

    @MockK private lateinit var syncedTabsStorage: SyncedTabsStorage

    @MockK private lateinit var accountManager: FxaAccountManager

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { syncedTabsStorage.stop() } just Runs
        every { accountManager.register(any(), owner = any(), autoPause = true) } just Runs
        every { context.applicationContext } returns mockk<FenixApplication> {
            every { components } returns mockk {
                every { backgroundServices.syncedTabsStorage } returns syncedTabsStorage
            }
        }
    }

    @Test
    fun `starts and stops syncedTabsStorage on user authentication`() {
        val observer = slot<AccountObserver>()
        SyncedTabsIntegration(context, accountManager).launch()
        verify { accountManager.register(capture(observer), owner = any(), autoPause = true) }

        every { syncedTabsStorage.start() } just Runs
        observer.captured.onAuthenticated(mockk(), mockk())
        verify { syncedTabsStorage.start() }

        every { syncedTabsStorage.stop() } just Runs
        observer.captured.onLoggedOut()
        verify { syncedTabsStorage.stop() }
    }
}
