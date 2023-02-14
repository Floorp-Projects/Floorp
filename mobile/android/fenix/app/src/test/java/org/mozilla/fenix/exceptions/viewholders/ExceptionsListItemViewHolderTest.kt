/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.exceptions.ExceptionsInteractor
import org.mozilla.fenix.helpers.MockkRetryTestRule

class ExceptionsListItemViewHolderTest {

    @MockK(relaxed = true)
    private lateinit var view: WidgetSiteItemView

    @MockK private lateinit var icons: BrowserIcons

    @MockK private lateinit var interactor: ExceptionsInteractor<Exception>

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        every { icons.loadIntoView(view.iconView, any()) } returns mockk()
    }

    @Test
    fun `sets url text and loads favicon - mozilla`() {
        ExceptionsListItemViewHolder(view, interactor, icons)
            .bind(Exception(), url = "mozilla.org")
        verify { view.setText(label = "mozilla.org", caption = null) }
        verify { icons.loadIntoView(view.iconView, IconRequest("mozilla.org")) }
    }

    @Test
    fun `sets url text and loads favicon - example`() {
        ExceptionsListItemViewHolder(view, interactor, icons)
            .bind(Exception(), url = "https://example.com/icon.svg")
        verify { view.setText(label = "https://example.com/icon.svg", caption = null) }
        verify { icons.loadIntoView(view.iconView, IconRequest("https://example.com/icon.svg")) }
    }

    @Test
    fun `delete button calls interactor`() {
        var clickListener: ((View) -> Unit)? = null
        val exception = Exception()
        every { view.setSecondaryButton(any(), any<Int>(), any()) } answers {
            clickListener = thirdArg()
        }
        ExceptionsListItemViewHolder(view, interactor, icons).bind(exception, url = "mozilla.org")

        every { interactor.onDeleteOne(exception) } just Runs
        assertNotNull(clickListener)
        clickListener!!(mockk())
        verify { interactor.onDeleteOne(exception) }
    }

    class Exception
}
