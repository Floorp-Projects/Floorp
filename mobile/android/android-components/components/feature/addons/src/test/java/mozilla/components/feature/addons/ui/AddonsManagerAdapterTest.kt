/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.NotYetSupportedSection
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.Section
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class AddonsManagerAdapterTest {
    @Test
    fun `createListWithSections() `() {
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList())

        val installedAddon: Addon = mock()
        val recommendedAddon: Addon = mock()
        val unsupportedAddon: Addon = mock()

        `when`(installedAddon.isInstalled()).thenReturn(true)
        `when`(installedAddon.isSupported()).thenReturn(true)
        `when`(unsupportedAddon.isInstalled()).thenReturn(true)
        `when`(unsupportedAddon.isSupported()).thenReturn(false)

        val addons = listOf(installedAddon, recommendedAddon, unsupportedAddon)

        assertEquals(0, adapter.itemCount)

        val itemsWithSections = adapter.createListWithSections(addons)

        assertEquals(5, itemsWithSections.size)
        assertEquals(
            R.string.mozac_feature_addons_installed_section,
            (itemsWithSections[0] as Section).title
        )
        assertEquals(installedAddon, itemsWithSections[1])
        assertEquals(
            R.string.mozac_feature_addons_recommended_section,
            (itemsWithSections[2] as Section).title
        )
        assertEquals(recommendedAddon, itemsWithSections[3])
        assertEquals(
            R.string.mozac_feature_addons_unsupported_section,
            (itemsWithSections[4] as NotYetSupportedSection).title
        )
    }
}
