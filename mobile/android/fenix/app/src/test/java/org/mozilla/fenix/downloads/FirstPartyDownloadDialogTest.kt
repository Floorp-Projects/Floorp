/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.app.Activity
import android.widget.FrameLayout
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.StartDownloadDialogLayoutBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class FirstPartyDownloadDialogTest {
    private val activity: Activity = Robolectric.buildActivity(Activity::class.java).create().get()

    @Before
    fun setup() {
        every { activity.settings().accessibilityServicesEnabled } returns false
    }

    @Test
    fun `GIVEN the size of the download is known WHEN setting it's View THEN bind all provided download data and show the download size`() {
        var wasPositiveActionDone = false
        var wasNegativeActionDone = false
        val contentSize = 5566L
        val dialog = spyk(
            FirstPartyDownloadDialog(
                activity = activity,
                filename = "Test",
                contentSize = contentSize,
                positiveButtonAction = { wasPositiveActionDone = true },
                negativeButtonAction = { wasNegativeActionDone = true },
            ),
        )
        every { dialog.dismiss() } just Runs
        val dialogParent = FrameLayout(testContext)
        dialog.container = dialogParent

        dialog.setupView()

        assertEquals(1, dialogParent.childCount)
        assertEquals(R.id.dialogLayout, dialogParent.getChildAt(0).id)
        val dialogBinding = dialog.binding as StartDownloadDialogLayoutBinding
        assertEquals(
            testContext.getString(
                R.string.mozac_feature_downloads_dialog_title2,
                contentSize.toMegabyteOrKilobyteString(),
            ),
            dialogBinding.title.text,
        )
        assertEquals("Test", dialogBinding.filename.text.toString())
        assertFalse(wasPositiveActionDone)
        assertFalse(wasNegativeActionDone)
        dialogBinding.downloadButton.callOnClick()
        verify { dialog.dismiss() }
        assertTrue(wasPositiveActionDone)
        dialogBinding.closeButton.callOnClick()
        verify(exactly = 2) { dialog.dismiss() }
        assertTrue(wasNegativeActionDone)
    }

    @Test
    fun `GIVEN the size of the download is not known WHEN setting it's View THEN bind all provided download data and show the download size`() {
        var wasPositiveActionDone = false
        var wasNegativeActionDone = false
        val contentSize = 0L
        val dialog = spyk(
            FirstPartyDownloadDialog(
                activity = activity,
                filename = "Test",
                contentSize = contentSize,
                positiveButtonAction = { wasPositiveActionDone = true },
                negativeButtonAction = { wasNegativeActionDone = true },
            ),
        )
        every { dialog.dismiss() } just Runs
        val dialogParent = FrameLayout(testContext)
        dialog.container = dialogParent

        dialog.setupView()

        assertEquals(1, dialogParent.childCount)
        assertEquals(R.id.dialogLayout, dialogParent.getChildAt(0).id)
        val dialogBinding = dialog.binding as StartDownloadDialogLayoutBinding
        assertEquals(
            testContext.getString(R.string.mozac_feature_downloads_dialog_download),
            dialogBinding.title.text,
        )
        assertEquals("Test", dialogBinding.filename.text.toString())
        assertFalse(wasPositiveActionDone)
        assertFalse(wasNegativeActionDone)
        dialogBinding.downloadButton.callOnClick()
        verify { dialog.dismiss() }
        assertTrue(wasPositiveActionDone)
        dialogBinding.closeButton.callOnClick()
        verify(exactly = 2) { dialog.dismiss() }
        assertTrue(wasNegativeActionDone)
    }
}
