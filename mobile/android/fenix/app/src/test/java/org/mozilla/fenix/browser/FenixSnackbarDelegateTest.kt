/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.View
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkObject
import io.mockk.verify
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FenixSnackbar.Companion.LENGTH_SHORT
import org.mozilla.fenix.helpers.MockkRetryTestRule

class FenixSnackbarDelegateTest {

    @MockK private lateinit var view: View

    @MockK(relaxed = true)
    private lateinit var snackbar: FenixSnackbar
    private lateinit var delegate: FenixSnackbarDelegate

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkObject(FenixSnackbar.Companion)

        delegate = FenixSnackbarDelegate(view)
        every {
            FenixSnackbar.make(view, LENGTH_SHORT, isDisplayedWithBrowserToolbar = true)
        } returns snackbar
        every { snackbar.setText(any()) } returns snackbar
        every { snackbar.setAction(any(), any()) } returns snackbar
        every { view.context.getString(R.string.app_name) } returns "Firefox"
        every { view.context.getString(R.string.edit_2) } returns "Edit password"
    }

    @After
    fun teardown() {
        unmockkObject(FenixSnackbar.Companion)
    }

    @Test
    fun `show with no listener nor action`() {
        delegate.show(
            snackBarParentView = mockk(),
            text = R.string.app_name,
            duration = 0,
            action = 0,
            listener = null,
        )

        verify { snackbar.setText("Firefox") }
        verify(exactly = 0) { snackbar.setAction(any(), any()) }
        verify { snackbar.show() }
    }

    @Test
    fun `show with listener but no action`() {
        delegate.show(
            snackBarParentView = mockk(),
            text = R.string.app_name,
            duration = 0,
            action = 0,
            listener = {},
        )

        verify { snackbar.setText("Firefox") }
        verify(exactly = 0) { snackbar.setAction(any(), any()) }
        verify { snackbar.show() }
    }

    @Test
    fun `show with action but no listener`() {
        delegate.show(
            snackBarParentView = mockk(),
            text = R.string.app_name,
            duration = 0,
            action = R.string.edit_2,
            listener = null,
        )

        verify { snackbar.setText("Firefox") }
        verify(exactly = 0) { snackbar.setAction(any(), any()) }
        verify { snackbar.show() }
    }

    @Test
    fun `show with listener and action`() {
        val listener = mockk<(View) -> Unit>(relaxed = true)
        delegate.show(
            snackBarParentView = mockk(),
            text = R.string.app_name,
            duration = 0,
            action = R.string.edit_2,
            listener = listener,
        )

        verify { snackbar.setText("Firefox") }
        verify {
            snackbar.setAction(
                "Edit password",
                withArg {
                    verify(exactly = 0) { listener(view) }
                    it.invoke()
                    verify { listener(view) }
                },
            )
        }
        verify { snackbar.show() }
    }
}
