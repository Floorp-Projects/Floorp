/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.DifferCallback
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.NotYetSupportedSection
import mozilla.components.feature.addons.ui.AddonsManagerAdapter.Section
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.io.IOException
import java.util.Locale

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AddonsManagerAdapterTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `createListWithSections`() {
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
            (itemsWithSections[0] as Section).title,
        )
        assertEquals(installedAddon, itemsWithSections[1])
        assertEquals(
            R.string.mozac_feature_addons_disabled_section,
            (itemsWithSections[2] as Section).title,
        )
        assertEquals(disabledAddon, itemsWithSections[3])
        assertEquals(
            R.string.mozac_feature_addons_recommended_section,
            (itemsWithSections[4] as Section).title,
        )
        assertEquals(recommendedAddon, itemsWithSections[5])
        assertEquals(
            R.string.mozac_feature_addons_unavailable_section,
            (itemsWithSections[6] as NotYetSupportedSection).title,
        )

        // Test if excluededAddonIDs are excluded from recommended section
        val excludedAddonIDs = listOf(recommendedAddon.id)
        val itemsWithSections2 = adapter.createListWithSections(addons, excludedAddonIDs)

        // The only recommended addon should be excluded, so the recommended section should be null
        // Section size should shrink from 7 to 6
        assertEquals(6, itemsWithSections2.size)
        // There should be no section between the titles of Recommended & NotYetSupported
        assertEquals(
            R.string.mozac_feature_addons_recommended_section,
            (itemsWithSections2[4] as Section).title,
        )
        assertEquals(
            R.string.mozac_feature_addons_unavailable_section,
            (itemsWithSections2[5] as NotYetSupportedSection).title,
        )
    }

    @Test
    fun `handle errors while fetching the add-on icon`() = runTestOnMain {
        val addon = mock<Addon>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val adapter = AddonsManagerAdapter(mockedCollectionProvider, mock(), emptyList())
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

    @Test
    fun `fetching the add-on icon from cache MUST NOT animate`() = runTestOnMain {
        val addon = mock<Addon>()
        val bitmap = mock<Bitmap>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val adapter = AddonsManagerAdapter(mockedCollectionProvider, mock(), emptyList())
        whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).thenReturn(bitmap)

        adapter.fetchIcon(addon, mockedImageView, scope).join()

        verify(mockedImageView).setImageDrawable(any())
    }

    @Test
    fun `fetching the add-on icon uncached MUST animate`() = runTestOnMain {
        val addon = mock<Addon>()
        val bitmap = mock<Bitmap>()
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val adapter = spy(AddonsManagerAdapter(mockedCollectionProvider, mock(), emptyList()))
        whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).thenAnswer {
            runBlocking {
                delay(1000)
            }
            bitmap
        }

        adapter.fetchIcon(addon, mockedImageView, scope).join()

        verify(adapter).setWithCrossFadeAnimation(mockedImageView, bitmap)
    }

    @Test
    fun `fall back to icon of installed extension`() = runTestOnMain {
        val addon = mock<Addon>()
        val installedState = mock<Addon.InstalledState>()
        val icon = mock<Bitmap>()
        whenever(installedState.icon).thenReturn(icon)
        whenever(addon.installedState).thenReturn(installedState)
        val mockedImageView = spy(ImageView(testContext))
        val mockedCollectionProvider = mock<AddonCollectionProvider>()
        val adapter = AddonsManagerAdapter(mockedCollectionProvider, mock(), emptyList())
        val captor = argumentCaptor<BitmapDrawable>()
        whenever(mockedCollectionProvider.getAddonIconBitmap(addon)).thenReturn(null)

        adapter.fetchIcon(addon, mockedImageView, scope).join()

        verify(mockedImageView).setImageDrawable(captor.capture())
        assertEquals(icon, captor.value.bitmap)
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
        val allowedInPrivateBrowsingLabel = ImageView(testContext)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val addonViewHolder = CustomViewHolder.AddonViewHolder(
            view = view,
            iconView = mock(),
            titleView = titleView,
            summaryView = summaryView,
            ratingView = mock(),
            ratingAccessibleView = ratingAccessibleView,
            userCountView = userCountView,
            addButton = addButton,
            allowedInPrivateBrowsingLabel = allowedInPrivateBrowsingLabel,
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
            translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary", "de" to "Kurzfassung", "es" to "resumen"),
        )

        whenever(titleView.context).thenReturn(testContext)
        whenever(summaryView.context).thenReturn(testContext)
        whenever(iconContainer.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            addonNameTextColor = android.R.color.transparent,
            addonSummaryTextColor = android.R.color.white,
        )
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, emptyList(), style)

        adapter.bindAddon(addonViewHolder, addon)

        verify(ratingAccessibleView).setText("4.50/5")
        verify(titleView).setText("name")
        verify(titleView).setTextColor(ContextCompat.getColor(testContext, style.addonNameTextColor!!))
        verify(summaryView).setText("summary")
        verify(summaryView).setTextColor(ContextCompat.getColor(testContext, style.addonSummaryTextColor!!))
        assertNotNull(addonViewHolder.itemView.tag)

        addonViewHolder.itemView.performClick()
        verify(addonsManagerAdapterDelegate).onAddonItemClicked(addon)
        addButton.performClick()
        verify(addonsManagerAdapterDelegate).onInstallAddonButtonClicked(addon)
    }

    @Test
    fun `bind section`() {
        val titleView: TextView = mock()
        val divider: View = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView, divider)
        val position = 0

        whenever(titleView.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock(),
        )
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section), position)

        verify(titleView).setText(R.string.mozac_feature_addons_disabled_section)
        verify(titleView).typeface = style.sectionsTypeFace
        verify(titleView).setTextColor(ContextCompat.getColor(testContext, style.sectionsTextColor!!))
        verify(divider).isVisible = style.visibleDividers && position != 0
    }

    @Test
    fun `default section divider visibility is 'GONE' for position 0`() {
        val titleView: TextView = mock()
        val divider: View = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView, divider)
        val position = 0

        whenever(titleView.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock(),
        )
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section), position)

        verify(divider).visibility = View.GONE
    }

    @Test
    fun `default section divider visibility is 'VISIBLE' for other position than 0`() {
        val titleView: TextView = mock()
        val divider: View = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView, divider)
        val position = 2

        whenever(titleView.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock(),
        )
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section), position)

        verify(divider).visibility = View.VISIBLE
    }

    @Test
    fun `section divider visibility is 'GONE' when set as such in style`() {
        val titleView: TextView = mock()
        val divider: View = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView, divider)
        val position = 2

        whenever(titleView.context).thenReturn(testContext)

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock(),
            visibleDividers = false,
        )
        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section), position)

        verify(divider).visibility = View.GONE
    }

    @Test
    fun `section divider style is set when arguments are passed in style`() {
        val titleView: TextView = mock()
        val divider: View = mock()
        val addonViewHolder = CustomViewHolder.SectionViewHolder(View(testContext), titleView, divider)
        val position = 2
        val dividerHeight = android.R.dimen.notification_large_icon_height
        val dividerColor = android.R.color.white

        whenever(titleView.context).thenReturn(testContext)
        whenever(divider.context).thenReturn(testContext)
        whenever(divider.layoutParams).thenReturn(mock())

        val style = AddonsManagerAdapter.Style(
            sectionsTextColor = android.R.color.black,
            sectionsTypeFace = mock(),
            visibleDividers = true,
            dividerColor = dividerColor,
            dividerHeight = dividerHeight,
        )

        val adapter = AddonsManagerAdapter(mock(), mock(), emptyList(), style)

        adapter.bindSection(addonViewHolder, Section(R.string.mozac_feature_addons_disabled_section), position)

        verify(divider).visibility = View.VISIBLE
        verify(divider).setBackgroundColor(dividerColor)
        verify(divider).layoutParams
        assertEquals(testContext.resources.getDimensionPixelOffset(dividerHeight), divider.layoutParams.height)
    }

    @Test
    fun `bind add-on with no available translatable name`() {
        Locale.setDefault(Locale.ENGLISH)
        val titleView: TextView = mock()
        val summaryView: TextView = mock()
        val view = View(testContext)
        val allowedInPrivateBrowsingLabel = ImageView(testContext)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val addonViewHolder = CustomViewHolder.AddonViewHolder(
            view = view,
            iconView = mock(),
            titleView = titleView,
            summaryView = summaryView,
            ratingView = mock(),
            ratingAccessibleView = mock(),
            userCountView = mock(),
            addButton = mock(),
            allowedInPrivateBrowsingLabel = allowedInPrivateBrowsingLabel,
        )
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, emptyList())

        adapter.bindAddon(addonViewHolder, addon)
        verify(titleView).setText("id")
        verify(summaryView).setVisibility(View.GONE)
    }

    @Test
    fun updateAddon() {
        var addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )
        val adapter = spy(AddonsManagerAdapter(mock(), mock(), listOf(addon)))

        assertEquals(addon, adapter.addonsMap[addon.id])

        addon = addon.copy(version = "newVersion")
        adapter.updateAddon(addon)

        assertEquals(addon.version, adapter.addonsMap[addon.id]!!.version)
        verify(adapter).submitList(any())
    }

    @Test
    fun updateAddons() {
        var addon1 = Addon(
            id = "addon1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )

        val addon2 = Addon(
            id = "addon2",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )
        val adapter = spy(AddonsManagerAdapter(mock(), mock(), listOf(addon1, addon2)))

        assertEquals(addon1, adapter.addonsMap[addon1.id])
        assertEquals(addon2, adapter.addonsMap[addon2.id])

        addon1 = addon1.copy(version = "newVersion")
        adapter.updateAddons(listOf(addon1, addon2))

        // Only addon1 must be updated
        assertEquals(addon1.version, adapter.addonsMap[addon1.id]!!.version)
        assertEquals(addon2, adapter.addonsMap[addon2.id])
        verify(adapter).submitList(any())
    }

    @Test
    fun differCallback() {
        var addon1 = Addon(
            id = "addon1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )

        var addon2 = Addon(
            id = "addon1",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )

        assertTrue(DifferCallback.areItemsTheSame(addon1, addon2))

        addon2 = addon2.copy(id = "addon2")

        assertFalse(DifferCallback.areItemsTheSame(addon1, addon2))

        addon2 = addon2.copy(id = "addon1")

        assertTrue(DifferCallback.areItemsTheSame(addon1, addon2))

        addon1 = addon1.copy(version = "newVersion")

        assertFalse(DifferCallback.areContentsTheSame(addon1, addon2))
    }

    @Test
    fun bindNotYetSupportedSection() {
        Locale.setDefault(Locale.ENGLISH)
        val addonsManagerAdapterDelegate: AddonsManagerAdapterDelegate = mock()
        val titleView: TextView = mock()
        val descriptionView: TextView = mock()
        val view = View(testContext)
        val unsupportedSectionViewHolder = CustomViewHolder.UnsupportedSectionViewHolder(
            view,
            titleView,
            descriptionView,
        )
        val unsupportedAddon = Addon(
            id = "id",
            installedState = Addon.InstalledState(
                id = "id",
                version = "version",
                optionsPageUrl = "optionsPageUrl",
                supported = false,
            ),
        )
        val unsupportedAddonTwo = Addon(
            id = "id2",
            installedState = Addon.InstalledState(
                id = "id2",
                version = "version2",
                optionsPageUrl = "optionsPageUrl2",
                supported = false,
            ),
        )
        val unsupportedAddons = arrayListOf(unsupportedAddon, unsupportedAddonTwo)
        val adapter = AddonsManagerAdapter(mock(), addonsManagerAdapterDelegate, unsupportedAddons)
        adapter.bindNotYetSupportedSection(unsupportedSectionViewHolder, mock())
        verify(unsupportedSectionViewHolder.descriptionView).setText(
            testContext.getString(R.string.mozac_feature_addons_unsupported_caption_plural, unsupportedAddons.size),
        )

        unsupportedSectionViewHolder.itemView.performClick()
        verify(addonsManagerAdapterDelegate).onNotYetSupportedSectionClicked(unsupportedAddons)
    }
}
