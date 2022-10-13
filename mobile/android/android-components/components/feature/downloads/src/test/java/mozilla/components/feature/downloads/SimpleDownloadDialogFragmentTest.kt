/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.Application
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.marginEnd
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
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
            "5MB.zip",
            "application/zip",
            5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36",
        )
        dialog = SimpleDownloadDialogFragment.newInstance()
    }

    @Test
    fun `when the positive button is clicked onStartDownload must be called`() {
        var isOnStartDownloadCalled = false

        val onStartDownload = {
            isOnStartDownloadCalled = true
        }

        val fragment = Mockito.spy(SimpleDownloadDialogFragment.newInstance())
        doNothing().`when`(fragment).dismiss()

        fragment.onStartDownload = onStartDownload
        fragment.testingContext = testContext

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        val downloadDialog = fragment.onCreateDialog(null)
        downloadDialog.show()

        val positiveButton = downloadDialog.findViewById<Button>(R.id.download_button)
        positiveButton.performClick()

        assertTrue(isOnStartDownloadCalled)
    }

    @Test
    fun `when the cancel button is clicked onCancelDownload must be called`() {
        var isDownloadCancelledCalled = false

        val onCancelDownload = {
            isDownloadCancelledCalled = true
        }

        val fragment = Mockito.spy(SimpleDownloadDialogFragment.newInstance())
        doNothing().`when`(fragment).dismiss()

        fragment.onCancelDownload = onCancelDownload
        fragment.testingContext = testContext

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        val downloadDialog = fragment.onCreateDialog(null)
        downloadDialog.show()

        val closeButton = downloadDialog.findViewById<ImageButton>(R.id.close_button)
        closeButton.performClick()

        assertTrue(isDownloadCancelledCalled)
    }

    @Test
    fun `dialog must adhere to promptsStyling`() {
        val promptsStyling = DownloadsFeature.PromptsStyling(
            gravity = Gravity.TOP,
            shouldWidthMatchParent = true,
            positiveButtonBackgroundColor = android.R.color.white,
            positiveButtonTextColor = android.R.color.black,
            positiveButtonRadius = 4f,
            fileNameEndMargin = 56,
        )

        val fragment = Mockito.spy(
            SimpleDownloadDialogFragment.newInstance(
                R.string.mozac_feature_downloads_dialog_title2,
                R.string.mozac_feature_downloads_dialog_download,
                0,
                promptsStyling,
            ),
        )
        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        val dialogAttributes = dialog.window!!.attributes
        val positiveButton = dialog.findViewById<Button>(R.id.download_button)
        val filename = dialog.findViewById<TextView>(R.id.filename)

        assertEquals(ContextCompat.getColor(testContext, promptsStyling.positiveButtonBackgroundColor!!), (positiveButton.background as GradientDrawable).color?.defaultColor)
        assertEquals(promptsStyling.positiveButtonRadius!!, (positiveButton.background as GradientDrawable).cornerRadius)
        assertEquals(ContextCompat.getColor(testContext, promptsStyling.positiveButtonTextColor!!), positiveButton.textColors.defaultColor)
        assertTrue(dialogAttributes.gravity == Gravity.TOP)
        assertTrue(dialogAttributes.width == ViewGroup.LayoutParams.MATCH_PARENT)
        assertTrue(filename.marginEnd == 56)
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}

class TestApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        setTheme(R.style.Theme_AppCompat)
    }
}
