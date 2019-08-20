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
import org.mozilla.fenix.BrowsingMode
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.R

class PrivateBrowsingButtonViewTest {

    private val enable = "Enable private browsing"
    private val disable = "Disable private browsing"

    private lateinit var button: View
    private lateinit var browsingModeManager: BrowsingModeManager

    @Before
    fun setup() {
        button = mockk(relaxed = true)
        browsingModeManager = mockk(relaxed = true)

        every { button.context.getString(R.string.content_description_private_browsing_button) } returns enable
        every { button.context.getString(R.string.content_description_disable_private_browsing_button) } returns disable
        every { browsingModeManager.mode } returns BrowsingMode.Normal
    }

    @Test
    fun `constructor sets contentDescription and click listener`() {
        val view = PrivateBrowsingButtonView(button, browsingModeManager) {}
        verify { button.context.getString(R.string.content_description_private_browsing_button) }
        verify { button.contentDescription = enable }
        verify { button.setOnClickListener(view) }

        every { browsingModeManager.mode } returns BrowsingMode.Private
        val privateView = PrivateBrowsingButtonView(button, browsingModeManager) {}
        verify { button.context.getString(R.string.content_description_disable_private_browsing_button) }
        verify { button.contentDescription = disable }
        verify { button.setOnClickListener(privateView) }
    }

    @Test
    fun `click listener calls onClick with inverted mode from normal mode`() {
        every { browsingModeManager.mode } returns BrowsingMode.Normal
        var mode: BrowsingMode? = null
        val view = PrivateBrowsingButtonView(button, browsingModeManager) { mode = it }

        view.onClick(button)

        assertEquals(BrowsingMode.Private, mode)
        verify { browsingModeManager.mode = BrowsingMode.Private }
    }

    @Test
    fun `click listener calls onClick with inverted mode from private mode`() {
        every { browsingModeManager.mode } returns BrowsingMode.Private
        var mode: BrowsingMode? = null
        val view = PrivateBrowsingButtonView(button, browsingModeManager) { mode = it }

        view.onClick(button)

        assertEquals(BrowsingMode.Normal, mode)
        verify { browsingModeManager.mode = BrowsingMode.Normal }
    }
}
