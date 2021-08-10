/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.net.Uri
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.addons.Addon
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.FragmentAddOnDetailsBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonDetailsViewTest {

    private lateinit var view: View
    private lateinit var binding: FragmentAddOnDetailsBinding
    private lateinit var interactor: AddonDetailsInteractor
    private lateinit var detailsView: AddonDetailsView
    private val baseAddon = Addon(
        id = "",
        translatableDescription = mapOf(
            Addon.DEFAULT_LOCALE to "Some blank addon\nwith a blank line"
        ),
        updatedAt = "2020-11-23T08:00:00Z"
    )

    @Before
    fun setup() {
        binding = FragmentAddOnDetailsBinding.inflate(LayoutInflater.from(testContext))
        view = binding.root
        interactor = mockk(relaxed = true)

        detailsView = AddonDetailsView(binding, interactor)
    }

    @Test
    fun `bind addons rating`() {
        detailsView.bind(
            baseAddon.copy(
                rating = null
            )
        )
        assertEquals(0f, binding.ratingView.rating)

        detailsView.bind(
            baseAddon.copy(
                rating = Addon.Rating(
                    average = 4.3f,
                    reviews = 100
                )
            )
        )
        assertEquals("4.30/5", binding.ratingView.contentDescription)
        assertEquals(4.5f, binding.ratingView.rating)
        assertEquals("100", binding.usersCount.text)
    }

    @Test
    fun `bind addons website`() {
        detailsView.bind(
            baseAddon.copy(
                siteUrl = "https://mozilla.org"
            )
        )

        binding.homePageLabel.performClick()

        verify { interactor.openWebsite(Uri.parse("https://mozilla.org")) }
    }

    @Test
    fun `bind addons last updated`() {
        detailsView.bind(baseAddon)

        assertEquals("Nov 23, 2020", binding.lastUpdatedText.text)
    }

    @Test
    fun `bind addons version`() {
        val addon1 = baseAddon.copy(
            version = "1.0.0",
            installedState = null
        )

        detailsView.bind(addon1)
        assertEquals("1.0.0", binding.versionText.text)
        binding.versionText.performLongClick()
        verify(exactly = 0) { interactor.showUpdaterDialog(addon1) }

        val addon2 = baseAddon.copy(
            version = "1.0.0",
            installedState = Addon.InstalledState(
                id = "",
                version = "2.0.0",
                optionsPageUrl = null
            )
        )
        detailsView.bind(addon2)
        assertEquals("2.0.0", binding.versionText.text)
        binding.versionText.performLongClick()
        verify { interactor.showUpdaterDialog(addon2) }
    }

    @Test
    fun `bind addons authors`() {
        val baseAuthor = Addon.Author("", "", "", "")
        detailsView.bind(
            baseAddon.copy(
                authors = listOf(
                    baseAuthor.copy(name = " Sarah Jane"),
                    baseAuthor.copy(name = "John Smith ")
                )
            )
        )

        assertEquals("Sarah Jane, John Smith", binding.authorText.text)
    }

    @Test
    fun `bind addons details`() {
        detailsView.bind(baseAddon)

        assertEquals(
            "Some blank addon\nwith a blank line",
            binding.details.text.toString()
        )
        assertTrue(binding.details.movementMethod is LinkMovementMethod)
    }
}
