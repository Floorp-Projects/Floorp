/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.ui

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
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
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import mozilla.components.feature.addons.ui.CustomViewHolder
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.IOException
import java.util.Locale

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
            R.string.mozac_feature_addons_enabled,
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
            R.string.mozac_feature_addons_unavailable_section,
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

    @Test
    fun `bind add-on`() {
        Locale.setDefault(Locale.ENGLISH)
        val iconContainer: CardView = mock()
        val titleView: TextView = mock()
        val summaryView: TextView = mock()
        val ratingAccessibleView: TextView = mock()
        val userCountView: TextView = mock()
        val addButton = ImageView(testContext)
        val view = View(testContext)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val addonViewHolder = CustomViewHolder.AddonViewHolder(
            view = view,
            iconContainer = iconContainer,
            iconView = mock(),
            titleView = titleView,
            summaryView = summaryView,
            ratingView = mock(),
            ratingAccessibleView = ratingAccessibleView,
            userCountView = userCountView,
            addButton = addButton
        )
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            rating = Addon.Rating(4.5f, 1000),
            createdAt = "",
            updatedAt = "",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "name", "de" to "Name", "es" to "nombre"),
            translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description", "de" to "Beschreibung", "es" to "descripción"),
            translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary", "de" to "Kurzfassung", "es" to "resumen")
        )

        whenever(titleView.context).thenReturn(testContext)
        whenever(summaryView.context).thenReturn(testContext)
        whenever(iconContainer.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            addonNameTextColor = android.R.color.transparent,
            addonSummaryTextColor = android.R.color.white,
            addonBackgroundIconColor = android.R.color.darker_gray
        )
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, emptyList(), style)

        adapter.bindAddon(addonViewHolder, addon)

        verify(ratingAccessibleView).setText("4.50/5")
        verify(titleView).setText("name")
        verify(titleView).setTextColor(ContextCompat.getColor(testContext, style.addonNameTextColor!!))
        verify(summaryView).setText("summary")
        verify(summaryView).setTextColor(ContextCompat.getColor(testContext, style.addonSummaryTextColor!!))
        verify(iconContainer).setCardBackgroundColor(ContextCompat.getColor(testContext, style.addonBackgroundIconColor!!))
        assertNotNull(addonViewHolder.itemView.tag)

        addonViewHolder.itemView.performClick()
        verify(addonsManagerAdapterDelegate).onAddonItemClicked(addon)
        addButton.performClick()
        verify(addonsManagerAdapterDelegate).onInstallAddonButtonClicked(addon)
    }

    @Test
    fun `bind section`() {
        val titleView: TextView = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView)

        whenever(titleView.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock()
        )
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section))

        verify(titleView).setText(R.string.mozac_feature_addons_disabled_section)
        verify(titleView).typeface = style.sectionsTypeFace
        verify(titleView).setTextColor(ContextCompat.getColor(testContext, style.sectionsTextColor!!))
    }

    @Test
    fun `bind add-on with no available translatable name`() {
        Locale.setDefault(Locale.ENGLISH)
        val titleView: TextView = mock()
        val summaryView: TextView = mock()
        val view = View(testContext)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val addonViewHolder = CustomViewHolder.AddonViewHolder(
            view = view,
            iconContainer = CardView(testContext),
            iconView = mock(),
            titleView = titleView,
            summaryView = summaryView,
            ratingView = mock(),
            ratingAccessibleView = mock(),
            userCountView = mock(),
            addButton = mock()
        )
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = ""
        )
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, emptyList())

        adapter.bindAddon(addonViewHolder, addon)
        verify(titleView).setText("id")
        verify(summaryView).setVisibility(View.GONE)
    }

    @Test
    fun bindNotYetSupportedSection() {
        Locale.setDefault(Locale.ENGLISH)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val titleView: TextView = mock()
        val descriptionView: TextView = mock()
        val view = View(testContext)
        val unsupportedSectionViewHolder = CustomViewHolder.UnsupportedSectionViewHolder(
            view, titleView, descriptionView
        )
        val unsupportedAddon = Addon(
            id = "id",
            installedState = Addon.InstalledState(
                id = "id",
                version = "version",
                optionsPageUrl = "optionsPageUrl",
                supported = false
            )
        )
        val unsupportedAddonTwo = Addon(
            id = "id2",
            installedState = Addon.InstalledState(
                id = "id2",
                version = "version2",
                optionsPageUrl = "optionsPageUrl2",
                supported = false
            )
        )
        val unsupportedAddons = arrayListOf(unsupportedAddon, unsupportedAddonTwo)
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, unsupportedAddons)
        adapter.bindNotYetSupportedSection(unsupportedSectionViewHolder, mock())
        verify(unsupportedSectionViewHolder.descriptionView).setText(
            testContext.getString(R.string.mozac_feature_addons_unsupported_caption_plural, unsupportedAddons.size)
        )

        unsupportedSectionViewHolder.itemView.performClick()
        verify(addonsManagerAdapterDelegate).onNotYetSupportedSectionClicked(unsupportedAddons)
    }
}
