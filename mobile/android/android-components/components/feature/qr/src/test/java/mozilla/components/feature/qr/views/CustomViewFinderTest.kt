/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr.views

import android.graphics.Rect
import androidx.core.content.ContextCompat
import androidx.core.text.HtmlCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.qr.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class CustomViewFinderTest {

    @Test
    fun `Static Layout is null on view init`() {
        val customViewFinder = spy(CustomViewFinder(testContext))
        assertNull(customViewFinder.scanMessageLayout)
    }

    @Test
    fun `calling setupMessage initializes the StaticLayout`() {
        val customViewFinder = spy(CustomViewFinder(testContext))
        val rect = mock(Rect::class.java)
        customViewFinder.viewFinderRectangle = rect

        CustomViewFinder.setMessage(R.string.mozac_feature_qr_scanner)
        assertNotNull(CustomViewFinder.scanMessageStringRes)
    }

    @Test
    fun `calling setupMessage with null value clears scan message `() {
        val customViewFinder = spy(CustomViewFinder(testContext))
        val rect = mock(Rect::class.java)
        customViewFinder.viewFinderRectangle = rect

        CustomViewFinder.setMessage(R.string.mozac_feature_qr_scanner)
        assertNotNull(CustomViewFinder.scanMessageStringRes)

        CustomViewFinder.setMessage(null)
        assertNull(CustomViewFinder.scanMessageStringRes)
    }

    @Test
    fun `message has the correct attributes`() {
        val customViewFinder = spy(CustomViewFinder(testContext))
        val rect = mock(Rect::class.java)
        customViewFinder.viewFinderRectangle = rect
        val mockWidth = 200
        val mockHeight = 300
        val testScanMessage = HtmlCompat.fromHtml(
            testContext.getString(R.string.mozac_feature_qr_scanner),
            HtmlCompat.FROM_HTML_MODE_LEGACY,
        )

        CustomViewFinder.setMessage(R.string.mozac_feature_qr_scanner)
        customViewFinder.computeViewFinderRect(mockWidth, mockHeight)

        assertEquals(
            ContextCompat.getColor(testContext, android.R.color.white),
            customViewFinder.scanMessageLayout?.paint?.color,
        )

        assertEquals(
            mockWidth * CustomViewFinder.DEFAULT_VIEWFINDER_WIDTH_RATIO,
            customViewFinder.scanMessageLayout?.width?.toFloat(),
        )

        assertEquals(
            testScanMessage,
            customViewFinder.scanMessageLayout?.text,
        )

        assertEquals(
            CustomViewFinder.SCAN_MESSAGE_TEXT_SIZE_SP,
            customViewFinder.scanMessageLayout?.paint?.textSize,
        )
    }

    @Test
    fun `setViewFinderColor sets the proper color to viewfinder`() {
        val customViewFinder = spy(CustomViewFinder(testContext))
        val rect = mock(Rect::class.java)
        customViewFinder.viewFinderRectangle = rect

        customViewFinder.setViewFinderColor(android.R.color.holo_red_dark)

        assertEquals(
            android.R.color.holo_red_dark,
            customViewFinder.viewFinderPaint.color,
        )
    }
}
