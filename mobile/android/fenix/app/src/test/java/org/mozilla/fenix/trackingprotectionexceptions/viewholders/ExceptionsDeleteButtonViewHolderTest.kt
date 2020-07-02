/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotectionexceptions.viewholders

import android.view.LayoutInflater
import android.view.View
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.delete_exceptions_button.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.trackingprotectionexceptions.ExceptionsInteractor
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExceptionsDeleteButtonViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: ExceptionsInteractor
    private lateinit var viewHolder: ExceptionsDeleteButtonViewHolder

    @Before
    fun setup() {
        val appCompatContext = ContextThemeWrapper(testContext, R.style.NormalTheme)
        view = LayoutInflater.from(appCompatContext)
            .inflate(ExceptionsDeleteButtonViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
        viewHolder = ExceptionsDeleteButtonViewHolder(view, interactor)
    }

    @Test
    fun `calls onDeleteAll on click`() {
        view.removeAllExceptions.performClick()

        verify { interactor.onDeleteAll() }
    }
}
