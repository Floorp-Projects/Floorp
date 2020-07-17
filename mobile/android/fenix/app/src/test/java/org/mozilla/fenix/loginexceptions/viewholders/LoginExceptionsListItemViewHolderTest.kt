/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions.viewholders

import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.feature.logins.exceptions.LoginException
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.loginexceptions.LoginExceptionsInteractor

class LoginExceptionsListItemViewHolderTest {

    private lateinit var view: View
    private lateinit var url: TextView
    private lateinit var deleteButton: ImageButton
    private lateinit var interactor: LoginExceptionsInteractor

    @Before
    fun setup() {
        url = mockk(relaxUnitFun = true)
        deleteButton = mockk(relaxUnitFun = true)
        view = mockk {
            every { findViewById<TextView>(R.id.webAddressView) } returns url
            every { findViewById<ImageButton>(R.id.delete_exception) } returns deleteButton
            every { findViewById<ImageView>(R.id.favicon_image) } returns mockk()
        }
        interactor = mockk()
    }

    @Test
    fun `sets url text`() {
        LoginExceptionsListItemViewHolder(view, interactor).bind(mockk {
            every { origin } returns "mozilla.org"
        })
        verify { url.text = "mozilla.org" }
    }

    @Test
    fun `delete button calls interactor`() {
        val slot = slot<View.OnClickListener>()
        val loginException = mockk<LoginException> {
            every { origin } returns "mozilla.org"
        }
        every { deleteButton.setOnClickListener(capture(slot)) } just Runs
        LoginExceptionsListItemViewHolder(view, interactor).bind(loginException)

        every { interactor.onDeleteOne(loginException) } just Runs
        slot.captured.onClick(mockk())
        verify { interactor.onDeleteOne(loginException) }
    }
}
