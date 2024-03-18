/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.app.Activity
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class StandardSnackbarErrorBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var activity: Activity
    private lateinit var snackbar: FenixSnackbar
    private lateinit var rootView: View

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkObject(FenixSnackbar)

        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(any(), any()) } returns mockk(relaxed = true)

        snackbar = mockk(relaxed = true)
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar
        rootView = mockk<ViewGroup>(relaxed = true)
        activity = mockk(relaxed = true) {
            every { findViewById<View>(android.R.id.content) } returns rootView
            every { getRootView() } returns rootView
        }
    }

    @After
    fun teardown() {
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `WHEN show standard snackbar error action dispatched THEN fenix snackbar should appear`() {
        val appStore = AppStore()
        val standardSnackbarError = StandardSnackbarErrorBinding(
            activity,
            appStore,
        )

        standardSnackbarError.start()
        appStore.dispatch(
            AppAction.UpdateStandardSnackbarErrorAction(
                StandardSnackbarError(
                    testContext.getString(R.string.unable_to_save_to_pdf_error),
                ),
            ),
        )
        appStore.waitUntilIdle()

        verify {
            snackbar.setText(testContext.getString(R.string.unable_to_save_to_pdf_error))
            snackbar.setButtonTextColor(
                ContextCompat.getColor(
                    activity,
                    R.color.fx_mobile_text_color_primary,
                ),
            )
            snackbar.setBackground(
                any(),
            )
            snackbar.setSnackBarTextColor(
                ContextCompat.getColor(
                    activity,
                    R.color.fx_mobile_text_color_warning,
                ),
            )
            snackbar.setAction(
                text = activity.getString(R.string.standard_snackbar_error_dismiss),
                any(),
            )
            snackbar.show()
        }
    }

    @Test
    fun `WHEN show standard snackbar error action dispatched and binding is stopped THEN fenix snackbar should appear when binding is again started`() {
        val appStore = AppStore()
        val standardSnackbarError = StandardSnackbarErrorBinding(
            activity,
            appStore,
        )

        standardSnackbarError.start()
        appStore.dispatch(
            AppAction.UpdateStandardSnackbarErrorAction(
                StandardSnackbarError(
                    testContext.getString(R.string.unable_to_save_to_pdf_error),
                ),
            ),
        )
        appStore.waitUntilIdle()

        standardSnackbarError.stop()

        standardSnackbarError.start()

        verify {
            snackbar.show()
        }
    }
}
