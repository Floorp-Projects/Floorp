/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.view.View.GONE
import android.view.View.VISIBLE
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.crashes.CrashContentView.Companion.TAP_INCREASE_DP
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class CrashContentViewTest {
    @Test
    fun `WHEN show is called THEN remember the controller, inflate and display the View`() {
        val view = spyk(CrashContentView(testContext))
        val controller: CrashReporterController = mockk()

        view.show(controller)

        assertTrue(view.controller === controller)
        verify {
            view.inflateViewIfNecessary()
            view.visibility = VISIBLE
        }
    }

    @Test
    fun `WHEN hide is called THEN remove the View from layout`() {
        val view = spyk(CrashContentView(testContext))

        view.hide()

        verify { view.visibility = GONE }
    }

    @Test
    fun `GIVEN the View is not shown WHEN needing to be shown THEN inflate the layout and bind all widgets`() {
        val controller: CrashReporterController = mockk(relaxed = true)
        val view = CrashContentView(testContext)
        view.controller = controller
        assertFalse(view.isBindingInitialized)

        mockkStatic("org.mozilla.fenix.ext.ViewKt") {
            view.inflateViewIfNecessary()

            assertTrue(view.isBindingInitialized)
            assertEquals(
                testContext.getString(R.string.tab_crash_title_2, testContext.getString(R.string.app_name)),
                view.binding.title.text,
            )
            verify {
                view.binding.restoreTabButton.increaseTapArea(TAP_INCREASE_DP)
                view.binding.closeTabButton.increaseTapArea(TAP_INCREASE_DP)
            }

            view.binding.sendCrashCheckbox.isChecked = true
            view.binding.restoreTabButton.callOnClick()
            verify { controller.handleCloseAndRestore(true) }

            view.binding.sendCrashCheckbox.isChecked = false
            view.binding.closeTabButton.callOnClick()
            verify { controller.handleCloseAndRemove(false) }
        }
    }

    @Test
    fun `GIVEN the View is not shown WHEN needing to be shown THEN delegate the process to helper methods`() {
        val view = spyk(CrashContentView(testContext))

        view.inflateViewIfNecessary()

        verify {
            view.inflate()
            view.bindViews()
        }
    }

    @Test
    fun `GIVEN the View is to already shown WHEN needing to be shown again THEN return early and avoid duplicating the widgets setup`() {
        val view = spyk(CrashContentView(testContext))
        view.inflate() // mock that the View is already inflated

        view.inflateViewIfNecessary() // try inflating it again

        verify(exactly = 1) { view.inflate() }
        verify(exactly = 0) { view.bindViews() }
    }
}
