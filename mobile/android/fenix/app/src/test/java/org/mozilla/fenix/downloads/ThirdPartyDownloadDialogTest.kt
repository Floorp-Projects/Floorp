/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.app.Activity
import android.widget.FrameLayout
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.feature.downloads.databinding.MozacDownloaderChooserPromptBinding
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class ThirdPartyDownloadDialogTest {
    private val activity: Activity = Robolectric.buildActivity(Activity::class.java).create().get()

    @Test
    fun `GIVEN a list of downloader apps WHEN setting it's View THEN bind all provided download data`() {
        var wasNegativeActionDone = false
        val dialog = spyk(
            ThirdPartyDownloadDialog(
                activity = activity,
                downloaderApps = listOf(mockk(), mockk()),
                onAppSelected = { /* cannot test the viewholder click */ },
                negativeButtonAction = { wasNegativeActionDone = true },
            ),
        )
        every { dialog.dismiss() } just Runs
        val dialogParent = FrameLayout(testContext)
        dialog.container = dialogParent

        dialog.setupView()

        assertEquals(1, dialogParent.childCount)
        assertEquals(R.id.relativeLayout, dialogParent.getChildAt(0).id)
        val dialogBinding = dialog.binding as MozacDownloaderChooserPromptBinding
        assertEquals(2, dialogBinding.appsList.adapter?.itemCount)
        dialogBinding.closeButton.callOnClick()
        assertTrue(wasNegativeActionDone)
        verify { dialog.dismiss() }
    }
}
