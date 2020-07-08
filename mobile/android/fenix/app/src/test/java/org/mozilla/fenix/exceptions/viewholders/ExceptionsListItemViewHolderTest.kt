/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.exception_item.view.*
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.exceptions.ExceptionsInteractor
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExceptionsListItemViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: ExceptionsInteractor
    private lateinit var viewHolder: ExceptionsListItemViewHolder

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(ExceptionsListItemViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
        viewHolder = ExceptionsListItemViewHolder(view, interactor)
    }

    @Test
    fun `bind url and icon`() {
        val exception = object : TrackingProtectionException {
            override val url = "https://example.com/icon.svg"
        }
        viewHolder.bind(exception)

        assertEquals(exception.url, view.webAddressView.text)
    }

    @Test
    fun `calls onDeleteOne on click`() {
        val exception = object : TrackingProtectionException {
            override val url = "https://example.com/icon.svg"
        }
        viewHolder.bind(exception)
        view.delete_exception.performClick()

        verify { interactor.onDeleteOne(exception) }
    }
}
