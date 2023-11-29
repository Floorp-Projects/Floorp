/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

class PrivateBrowsingButtonViewTest {

    private val enable = "Enable private browsing"
    private val disable = "Disable private browsing"

    private lateinit var button: View

    @Before
    fun setup() {
        button = mockk(relaxed = true)

        every { button.context.getString(R.string.content_description_private_browsing_button) } returns enable
        every { button.context.getString(R.string.content_description_disable_private_browsing_button) } returns disable
    }

    @Test
    fun `constructor sets contentDescription and click listener`() {
        val view = PrivateBrowsingButtonView(button, BrowsingMode.Normal) {}
        verify { button.context.getString(R.string.content_description_private_browsing_button) }
        verify { button.contentDescription = enable }
        verify { button.setOnClickListener(view) }

        val privateView = PrivateBrowsingButtonView(button, BrowsingMode.Private) {}
        verify { button.context.getString(R.string.content_description_disable_private_browsing_button) }
        verify { button.contentDescription = disable }
        verify { button.setOnClickListener(privateView) }
    }

    @Test
    fun `click listener calls onClick with inverted mode from normal mode`() {
        var mode = BrowsingMode.Normal
        val view = PrivateBrowsingButtonView(button, mode) { mode = it }

        view.onClick(button)

        assertEquals(BrowsingMode.Private, mode)
    }

    @Test
    fun `click listener calls onClick with inverted mode from private mode`() {
        var mode = BrowsingMode.Private
        val view = PrivateBrowsingButtonView(button, mode) { mode = it }

        view.onClick(button)

        assertEquals(BrowsingMode.Normal, mode)
    }
}
