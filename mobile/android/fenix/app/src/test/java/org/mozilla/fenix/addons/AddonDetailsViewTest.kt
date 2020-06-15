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
import kotlinx.android.synthetic.main.fragment_add_on_details.view.*
import mozilla.components.feature.addons.Addon
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonDetailsViewTest {

    private lateinit var view: View
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
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_add_on_details, null)
        interactor = mockk(relaxed = true)

        detailsView = AddonDetailsView(view, interactor)
    }

    @Test
    fun `bind addons rating`() {
        detailsView.bind(baseAddon.copy(
            rating = null
        ))
        assertEquals(0f, view.rating_view.rating)

        detailsView.bind(baseAddon.copy(
            rating = Addon.Rating(
                average = 4.3f,
                reviews = 100
            )
        ))
        assertEquals("4.30/5", view.rating_view.contentDescription)
        assertEquals(4.5f, view.rating_view.rating)
        assertEquals("100", view.users_count.text)
    }

    @Test
    fun `bind addons website`() {
        detailsView.bind(baseAddon.copy(
            siteUrl = "https://mozilla.org"
        ))

        view.home_page_label.performClick()

        verify { interactor.openWebsite(Uri.parse("https://mozilla.org")) }
    }

    @Test
    fun `bind addons last updated`() {
        detailsView.bind(baseAddon)

        assertEquals("Nov 23, 2020", view.last_updated_text.text)
    }

    @Test
    fun `bind addons version`() {
        detailsView.bind(baseAddon.copy(
            version = "1.0.0",
            installedState = null
        ))
        assertEquals("1.0.0", view.version_text.text)
        view.version_text.performLongClick()
        verify(exactly = 0) { interactor.showUpdaterDialog(any()) }

        detailsView.bind(baseAddon.copy(
            version = "1.0.0",
            installedState = Addon.InstalledState(
                id = "",
                version = "2.0.0",
                optionsPageUrl = null
            )
        ))
        assertEquals("2.0.0", view.version_text.text)
        view.version_text.performLongClick()
        verify { interactor.showUpdaterDialog(any()) }
    }

    @Test
    fun `bind addons authors`() {
        val baseAuthor = Addon.Author("", "", "", "")
        detailsView.bind(baseAddon.copy(
            authors = listOf(
                baseAuthor.copy(name = " Sarah Jane"),
                baseAuthor.copy(name = "John Smith ")
            )
        ))

        assertEquals("Sarah Jane, John Smith", view.author_text.text)
    }

    @Test
    fun `bind addons details`() {
        detailsView.bind(baseAddon)

        assertEquals(
            "Some blank addon\nwith a blank line",
            view.details.text.toString()
        )
        assertTrue(view.details.movementMethod is LinkMovementMethod)
    }
}
