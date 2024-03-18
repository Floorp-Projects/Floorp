/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import com.google.android.material.button.MaterialButton
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsInteractor

class ExceptionsDeleteButtonViewHolderTest {

    @MockK private lateinit var view: View

    @MockK private lateinit var deleteButton: MaterialButton

    @MockK private lateinit var interactor: ExceptionsInteractor<Unit>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { view.findViewById<MaterialButton>(R.id.removeAllExceptions) } returns deleteButton
    }

    @Test
    fun `delete button calls interactor`() {
        val slot = slot<View.OnClickListener>()
        every { deleteButton.setOnClickListener(capture(slot)) } just Runs
        ExceptionsDeleteButtonViewHolder(view, interactor)

        every { interactor.onDeleteAll() } just Runs
        slot.captured.onClick(mockk())
        verify { interactor.onDeleteAll() }
    }
}
