/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions.viewholders

import android.view.View
import com.google.android.material.button.MaterialButton
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.loginexceptions.LoginExceptionsInteractor

class LoginExceptionsDeleteButtonViewHolderTest {

    private lateinit var view: View
    private lateinit var deleteButton: MaterialButton
    private lateinit var interactor: LoginExceptionsInteractor

    @Before
    fun setup() {
        deleteButton = mockk()
        view = mockk {
            every { findViewById<MaterialButton>(R.id.removeAllExceptions) } returns deleteButton
        }
        interactor = mockk()
    }

    @Test
    fun `delete button calls interactor`() {
        val slot = slot<View.OnClickListener>()
        every { deleteButton.setOnClickListener(capture(slot)) } just Runs
        LoginExceptionsDeleteButtonViewHolder(view, interactor)

        every { interactor.onDeleteAll() } just Runs
        slot.captured.onClick(mockk())
        verify { interactor.onDeleteAll() }
    }
}
