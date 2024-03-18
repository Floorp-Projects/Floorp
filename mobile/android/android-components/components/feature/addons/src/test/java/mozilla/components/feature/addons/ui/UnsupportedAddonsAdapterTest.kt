/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.widget.ImageButton
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class UnsupportedAddonsAdapterTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `removing successfully notifies the adapter item changed`() {
        val addonManager: AddonManager = mock()
        val unsupportedAddonsAdapterDelegate: UnsupportedAddonsAdapterDelegate = mock()
        val addonOne = Addon("id1")
        val addonTwo = Addon("id2")
        val unsupportedAddons = listOf(addonOne, addonTwo)

        val adapter = spy(
            UnsupportedAddonsAdapter(
                addonManager,
                unsupportedAddonsAdapterDelegate,
                unsupportedAddons,
            ),
        )

        adapter.removeUninstalledAddon(addonOne)
        verify(unsupportedAddonsAdapterDelegate, times(1)).onUninstallSuccess()
        verify(adapter, times(1)).notifyDataSetChanged()
        assertEquals(1, adapter.itemCount)

        adapter.removeUninstalledAddon(addonTwo)
        verify(unsupportedAddonsAdapterDelegate, times(2)).onUninstallSuccess()
        verify(adapter, times(2)).notifyDataSetChanged()
        assertEquals(0, adapter.itemCount)

        adapter.removeUninstalledAddon(addonTwo)
        verify(unsupportedAddonsAdapterDelegate, times(2)).onUninstallSuccess()
        verify(adapter, times(2)).notifyDataSetChanged()
    }

    @Test
    fun `uninstalling action disables all remove buttons`() {
        val removeButtonOne = ImageButton(testContext)
        val unsupportedViewHolderOne = UnsupportedAddonsAdapter.UnsupportedAddonViewHolder(
            view = mock(),
            iconView = mock(),
            titleView = mock(),
            removeButton = removeButtonOne,
        )
        val removeButtonTwo = ImageButton(testContext)
        val unsupportedViewHolderTwo = UnsupportedAddonsAdapter.UnsupportedAddonViewHolder(
            view = mock(),
            iconView = mock(),
            titleView = mock(),
            removeButton = removeButtonTwo,
        )
        val addonManager: AddonManager = mock()
        val addonOne = Addon("id1")
        val addonTwo = Addon("id2")
        val unsupportedAddons = mapOf(
            unsupportedViewHolderOne to addonOne,
            unsupportedViewHolderTwo to addonTwo,
        )
        val adapter = spy(
            UnsupportedAddonsAdapter(
                addonManager,
                mock(),
                unsupportedAddons.values.toList(),
            ),
        )

        // mock the adapter.notifyDataSetChanged() behavior
        whenever(adapter.notifyDataSetChanged()).thenAnswer {
            unsupportedAddons.forEach { addonEntry ->
                val addonPair = addonEntry.toPair()
                adapter.bindRemoveButton(addonPair.first, addonPair.second)
            }
        }

        val onSuccessCaptor = argumentCaptor<(() -> Unit)>()
        adapter.bindRemoveButton(unsupportedViewHolderOne, addonOne)
        assertFalse(adapter.pendingUninstall)
        removeButtonOne.performClick()
        assertTrue(adapter.pendingUninstall)
        verify(adapter, times(1)).notifyDataSetChanged()
        // All the visible remove buttons in the adapter should be disabled
        assertFalse(removeButtonOne.isEnabled)
        assertFalse(removeButtonTwo.isEnabled)

        verify(addonManager).uninstallAddon(any(), onSuccessCaptor.capture(), any())
        onSuccessCaptor.value.invoke()
        assertFalse(adapter.pendingUninstall)
        verify(adapter, times(2)).notifyDataSetChanged()
        // All the visible remove buttons in the adapter should be enabled after uninstall complete
        assertTrue(removeButtonOne.isEnabled)
        assertTrue(removeButtonTwo.isEnabled)
    }
}
