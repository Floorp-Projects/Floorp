/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsInteractor

class ExceptionsListItemViewHolderTest {

    @MockK private lateinit var view: View
    @MockK(relaxUnitFun = true) private lateinit var url: TextView
    @MockK(relaxUnitFun = true) private lateinit var deleteButton: ImageButton
    @MockK private lateinit var favicon: ImageView
    @MockK private lateinit var icons: BrowserIcons
    @MockK private lateinit var interactor: ExceptionsInteractor<Exception>

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        every { view.findViewById<TextView>(R.id.webAddressView) } returns url
        every { view.findViewById<ImageButton>(R.id.delete_exception) } returns deleteButton
        every { view.findViewById<ImageView>(R.id.favicon_image) } returns favicon
        every { icons.loadIntoView(favicon, any()) } returns mockk()
    }

    @Test
    fun `sets url text and loads favicon - mozilla`() {
        ExceptionsListItemViewHolder(view, interactor, icons)
            .bind(Exception(), url = "mozilla.org")
        verify { url.text = "mozilla.org" }
        verify { icons.loadIntoView(favicon, IconRequest("mozilla.org")) }
    }

    @Test
    fun `sets url text and loads favicon - example`() {
        ExceptionsListItemViewHolder(view, interactor, icons)
            .bind(Exception(), url = "https://example.com/icon.svg")
        verify { url.text = "https://example.com/icon.svg" }
        verify { icons.loadIntoView(favicon, IconRequest("https://example.com/icon.svg")) }
    }

    @Test
    fun `delete button calls interactor`() {
        val slot = slot<View.OnClickListener>()
        val exception = Exception()
        every { deleteButton.setOnClickListener(capture(slot)) } just Runs
        ExceptionsListItemViewHolder(view, interactor, icons).bind(exception, url = "mozilla.org")

        every { interactor.onDeleteOne(exception) } just Runs
        slot.captured.onClick(mockk())
        verify { interactor.onDeleteOne(exception) }
    }

    class Exception
}
