/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ui

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DownloaderAppAdapterTest {

    @Test
    fun `bind apps`() {
        val nameLabel = spy(TextView(testContext))
        val iconImage = spy(ImageView(testContext))
        val ourApp = DownloaderApp(name = "app", packageName = "thridparty.app", resolver = mock(), activityName = "", url = "", contentType = null)
        val apps = listOf(ourApp)
        val view = View(testContext)
        var appSelected = false
        val viewHolder = DownloaderAppViewHolder(view, nameLabel, iconImage)

        val adapter = DownloaderAppAdapter(testContext, apps) {
            appSelected = true
        }

        adapter.onBindViewHolder(viewHolder, 0)
        view.performClick()

        verify(nameLabel).text = ourApp.name
        verify(iconImage).setImageDrawable(any())
        assertTrue(appSelected)
        assertEquals(ourApp, view.tag)
    }
}
