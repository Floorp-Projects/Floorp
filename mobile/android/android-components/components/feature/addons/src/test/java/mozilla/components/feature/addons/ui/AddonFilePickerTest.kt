/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.mozilla.components.feature.addons.ui

import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import androidx.activity.result.ActivityResultCaller
import androidx.activity.result.ActivityResultLauncher
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webextension.InstallationMethod
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.ui.AddonFilePicker
import mozilla.components.feature.addons.ui.AddonOpenDocument
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AddonFilePickerTest {

    private lateinit var filePicker: AddonFilePicker
    private lateinit var addonManager: AddonManager

    @Before
    fun setup() {
        addonManager = mock()
        filePicker = spy(
            AddonFilePicker(testContext, addonManager),
        )
    }

    @Test
    fun `WHEN registerForResults is called THEN register AddonOpenDocument`() {
        val resultCaller: ActivityResultCaller = mock()

        whenever(resultCaller.registerForActivityResult(any<AddonOpenDocument>(), any())).thenReturn(mock())

        filePicker.registerForResults(resultCaller)

        verify(resultCaller).registerForActivityResult(any<AddonOpenDocument>(), any())
    }

    @Test
    fun `WHEN launch is called THEN delegate to the activityLauncher`() {
        val activityLauncher: ActivityResultLauncher<Array<String>> = mock()

        filePicker.activityLauncher = activityLauncher

        val wasOpened = filePicker.launch()

        verify(activityLauncher).launch(any())
        assertTrue(wasOpened)
    }

    @Test
    fun `GIVEN a file picker is not present WHEN launch is called THEN return false`() {
        val activityLauncher: ActivityResultLauncher<Array<String>> = mock()

        filePicker.activityLauncher = activityLauncher
        whenever(activityLauncher.launch(any())).thenThrow(ActivityNotFoundException())

        val wasOpened = filePicker.launch()

        verify(activityLauncher).launch(any())
        assertFalse(wasOpened)
    }

    @Test
    fun `WHEN handleUriSelected is called THEN return false`() {
        val uri: Uri = mock()

        doReturn("file:///data/data/XPIs/addon.xpi").`when`(filePicker).convertToFileUri(uri)

        filePicker.handleUriSelected(uri)

        verify(addonManager).installAddon(
            url = any<String>(),
            installationMethod = eq(InstallationMethod.FROM_FILE),
            onSuccess = any(),
            onError = any(),
        )
    }

    @Test
    fun `WHEN creating an intent from a AddonOpenDocument THEN it contains mime types for zips and xpi files`() {
        val contractResult = AddonOpenDocument()

        val intent = contractResult.createIntent(testContext, arrayOf())

        val mimeTypes = intent.extras!!.getStringArray(Intent.EXTRA_MIME_TYPES)
        assertArrayEquals(mimeTypes, arrayOf("application/x-xpinstall", "application/zip"))
    }
}
