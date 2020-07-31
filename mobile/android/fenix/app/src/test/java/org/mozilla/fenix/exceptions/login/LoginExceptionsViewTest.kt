/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import io.mockk.mockk
import kotlinx.android.synthetic.main.component_exceptions.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class LoginExceptionsViewTest {

    private lateinit var parent: ViewGroup
    private lateinit var interactor: LoginExceptionsInteractor
    private lateinit var view: LoginExceptionsView

    @Before
    fun setup() {
        parent = FrameLayout(testContext)
        interactor = mockk()
        view = LoginExceptionsView(
            parent,
            interactor
        )
    }

    @Test
    fun `sets empty message text`() {
        assertEquals(
            "Logins and passwords that are not saved will be shown here.",
            view.exceptions_empty_message.text
        )
        assertTrue(view.exceptions_list.adapter is LoginExceptionsAdapter)
        assertTrue(view.exceptions_list.layoutManager is LinearLayoutManager)
    }

    @Test
    fun `hide list when there are no items`() {
        view.update(emptyList())

        assertTrue(view.exceptions_empty_view.isVisible)
        assertFalse(view.exceptions_list.isVisible)
    }

    @Test
    fun `shows list when there are items`() {
        view.update(listOf(mockk()))

        assertFalse(view.exceptions_empty_view.isVisible)
        assertTrue(view.exceptions_list.isVisible)
    }
}
