/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.ui.UnsupportedAddonsAdapter
import mozilla.components.feature.addons.ui.UnsupportedAddonsAdapterDelegate
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class UnsupportedAddonsAdapterTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Test
    fun `removing successfully notifies the adapter item changed`() {
        val addonManager: AddonManager = mock()
        val unsupportedAddonsAdapterDelegate: UnsupportedAddonsAdapterDelegate = mock()
        val unsupportedAddons = listOf(Addon("id1"), Addon("id2"))

        val adapter = spy(
            UnsupportedAddonsAdapter(
                addonManager,
                unsupportedAddonsAdapterDelegate,
                unsupportedAddons
            )
        )

        adapter.removeUninstalledAddonAtPosition(0)
        testDispatcher.advanceUntilIdle()
        verify(unsupportedAddonsAdapterDelegate, times(1)).onUninstallSuccess()
        verify(adapter, times(1)).notifyItemRemoved(0)
        assertEquals(1, adapter.itemCount)

        adapter.removeUninstalledAddonAtPosition(0)
        testDispatcher.advanceUntilIdle()
        verify(unsupportedAddonsAdapterDelegate, times(2)).onUninstallSuccess()
        verify(adapter, times(2)).notifyItemRemoved(0)
        assertEquals(0, adapter.itemCount)

        adapter.removeUninstalledAddonAtPosition(0)
        testDispatcher.advanceUntilIdle()
        verify(unsupportedAddonsAdapterDelegate, times(2)).onUninstallSuccess()
        verify(adapter, times(2)).notifyItemRemoved(0)
    }
}
