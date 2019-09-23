/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.Application
import android.content.DialogInterface.BUTTON_POSITIVE
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class SimpleDownloadDialogFragmentTest {

    private lateinit var dialog: SimpleDownloadDialogFragment
    private lateinit var download: DownloadState
    private lateinit var mockFragmentManager: FragmentManager

    @Before
    fun setup() {
        mockFragmentManager = mock()
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "5MB.zip", "application/zip", 5242880,
            "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
        )
        dialog = SimpleDownloadDialogFragment.newInstance()
    }

    @Test
    fun `when the positive button is clicked onStartDownload must be called`() {
        var isOnStartDownloadCalled = false

        val onStartDownload = {
            isOnStartDownloadCalled = true
        }

        dialog.onStartDownload = onStartDownload
        dialog.testingContext = testContext

        performClick(BUTTON_POSITIVE)

        assertTrue(isOnStartDownloadCalled)
    }

    private fun performClick(buttonID: Int) {
        val alert = dialog.onCreateDialog(null)

        alert.show()

        val negativeButton = (alert as AlertDialog).getButton(buttonID)

        negativeButton.performClick()
    }
}

class TestApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        setTheme(R.style.Theme_AppCompat)
    }
}