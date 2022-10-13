/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ui

import android.app.Application
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatImageButton
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.R
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DownloadAppChooserDialogTest {

    private lateinit var dialog: DownloadAppChooserDialog
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
        dialog = DownloadAppChooserDialog.newInstance()
    }

    @Test
    fun `when an app is selected onAppSelected must be called`() {
        var onAppSelectedWasCalled = false
        val ourApp = DownloaderApp(name = "app", packageName = testContext.packageName, resolver = mock(), activityName = "", url = "", contentType = null)
        val apps = listOf(ourApp)

        val onAppSelected: ((DownloaderApp) -> Unit) = {
            onAppSelectedWasCalled = true
        }

        val fragment = spy(DownloadAppChooserDialog.newInstance())
        doNothing().`when`(fragment).dismiss()

        fragment.setApps(apps)
        fragment.onAppSelected = onAppSelected

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        val downloadDialog = fragment.onCreateDialog(null)
        downloadDialog.show()

        val adapter = downloadDialog.findViewById<RecyclerView>(R.id.apps_list).adapter
        val holder = adapter!!.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        holder.itemView.performClick()

        assertTrue(onAppSelectedWasCalled)
    }

    @Test
    fun `when the cancel button is clicked onDismiss must be called`() {
        var isDismissCalled = false

        val onDismiss = {
            isDismissCalled = true
        }

        val fragment = spy(DownloadAppChooserDialog.newInstance())
        doNothing().`when`(fragment).dismiss()

        fragment.onDismiss = onDismiss

        doReturn(testContext).`when`(fragment).requireContext()
        @Suppress("DEPRECATION")
        doReturn(mockFragmentManager()).`when`(fragment).requireFragmentManager()

        val downloadDialog = fragment.onCreateDialog(null)
        downloadDialog.show()

        val closeButton = downloadDialog.findViewById<AppCompatImageButton>(R.id.close_button)
        closeButton.performClick()

        assertTrue(isDismissCalled)
    }

    @Test
    fun `dialog must adhere to promptsStyling`() {
        val fragment = spy(DownloadAppChooserDialog.newInstance(Gravity.TOP, true))
        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        val dialogAttributes = dialog.window!!.attributes

        assertTrue(dialogAttributes.gravity == Gravity.TOP)
        assertTrue(dialogAttributes.width == ViewGroup.LayoutParams.MATCH_PARENT)
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
