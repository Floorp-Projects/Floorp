/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.NotYetSupportedSection
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.Section
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.IOException

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AddonsManagerAdapterTest {

    private val testDispatcher = TestCoroutineDispatcher()
    private val scope = TestCoroutineScope(testDispatcher)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Test
    fun `createListWithSections() `() {
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList())

        val installedAddon: Addon = mock()
        val recommendedAddon: Addon = mock()
        val unsupportedAddon: Addon = mock()
        val disabledAddon: Addon = mock()

        `when`(installedAddon.isInstalled()).thenReturn(true)
        `when`(installedAddon.isEnabled()).thenReturn(true)
        `when`(installedAddon.isSupported()).thenReturn(true)
        `when`(unsupportedAddon.isInstalled()).thenReturn(true)
        `when`(unsupportedAddon.isSupported()).thenReturn(false)
        `when`(disabledAddon.isEnabled()).thenReturn(false)
        `when`(disabledAddon.isInstalled()).thenReturn(true)
        `when`(disabledAddon.isSupported()).thenReturn(true)

        val addons = listOf(installedAddon, recommendedAddon, unsupportedAddon, disabledAddon)

        assertEquals(0, adapter.itemCount)

        val itemsWithSections = adapter.createListWithSections(addons)

        assertEquals(7, itemsWithSections.size)
        assertEquals(
            R.string.mozac_feature_addons_installed_section,
            (itemsWithSections[0] as Section).title
        )
        assertEquals(installedAddon, itemsWithSections[1])
        assertEquals(
            R.string.mozac_feature_addons_disabled_section,
            (itemsWithSections[2] as Section).title
        )
        assertEquals(disabledAddon, itemsWithSections[3])
        assertEquals(
            R.string.mozac_feature_addons_recommended_section,
            (itemsWithSections[4] as Section).title
        )
        assertEquals(recommendedAddon, itemsWithSections[5])
        assertEquals(
            R.string.mozac_feature_addons_unsupported_section,
            (itemsWithSections[6] as NotYetSupportedSection).title
        )
    }

    @Test
    fun `handle errors while fetching the add-on icon() `() {
        val addon = mock<Addon>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val adapter = AddonsManagerAdapter(mockedCollectionProvider, mock(), emptyList())

        runBlocking {
            whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).then {
                throw IOException("Request failed")
            }
            try {
                adapter.fetchIcon(addon, mockedImageView, scope).join()
                verify(mockedImageView).setColorFilter(anyInt())
            } catch (e: IOException) {
                fail("The exception must be handle in the adapter")
            }
        }
    }
}
