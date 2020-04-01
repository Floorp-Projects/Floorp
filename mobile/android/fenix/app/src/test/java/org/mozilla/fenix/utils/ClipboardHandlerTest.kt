/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(FenixRobolectricTestRunner::class)
class ClipboardHandlerTest {

    private val clipboardUrl = "https://www.mozilla.org"
    private val clipboardText = "Mozilla"
    private lateinit var clipboard: ClipboardManager
    private lateinit var clipboardHandler: ClipboardHandler

    @Before
    fun setup() {
        clipboard = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clipboardHandler = ClipboardHandler(testContext)
    }

    @Test
    fun getText() {
        assertEquals(null, clipboardHandler.text)

        clipboard.primaryClip = ClipData.newPlainText("Text", clipboardText)
        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun setText() {
        assertEquals(null, clipboardHandler.text)

        clipboardHandler.text = clipboardText
        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun getUrl() {
        assertEquals(null, clipboardHandler.url)

        clipboard.primaryClip = ClipData.newPlainText("Text", clipboardUrl)
        assertEquals(clipboardUrl, clipboardHandler.url)
    }

    @Test
    fun getUrlfromTextUrlMIME() {
        assertEquals(null, clipboardHandler.url)

        clipboard.primaryClip = ClipData.newHtmlText("Html", clipboardUrl, clipboardUrl)
        assertEquals(clipboardUrl, clipboardHandler.url)
    }
}
