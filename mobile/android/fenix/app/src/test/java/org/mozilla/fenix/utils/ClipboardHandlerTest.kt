/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import io.mockk.every
import io.mockk.mockkObject
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.SafeUrl
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

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

        clipboard.setPrimaryClip(ClipData.newPlainText("Text", clipboardText))
        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun setText() {
        assertEquals(null, clipboardHandler.text)

        clipboardHandler.text = clipboardText
        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun `extract url from plaintext mime clipboard clip`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(ClipData.newPlainText("Text", clipboardUrl))
        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `extract url from html mime clipboard clip`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(ClipData.newHtmlText("Html", clipboardUrl, clipboardUrl))
        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `extract url from url mime clipboard clip`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(
            ClipData(clipboardUrl, arrayOf("text/x-moz-url"), ClipData.Item(clipboardUrl)),
        )
        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `text should return firstSafePrimaryClipItemText`() {
        val safeResult = "safeResult"
        clipboard.setPrimaryClip(ClipData.newPlainText(clipboardUrl, clipboardText))
        clipboardHandler = spyk(clipboardHandler)
        every { clipboardHandler getProperty "firstSafePrimaryClipItemText" } propertyType String::class returns safeResult

        val result = clipboardHandler.text

        verify { clipboardHandler getProperty "firstSafePrimaryClipItemText" }
        assertEquals(safeResult, result)
    }

    @Test
    fun `firstSafePrimaryClipItemText should return the result of SafeUrl#stripUnsafeUrlSchemes`() {
        mockkObject(SafeUrl)
        try {
            every { SafeUrl.stripUnsafeUrlSchemes(any(), any()) } returns "safeResult"
            clipboard.setPrimaryClip(ClipData.newHtmlText("Html", clipboardUrl, clipboardUrl))

            val result = clipboardHandler.firstSafePrimaryClipItemText

            verify { SafeUrl.stripUnsafeUrlSchemes(testContext, clipboardUrl) }
            assertEquals("safeResult", result)
        } finally {
            unmockkObject(SafeUrl)
        }
    }
}
