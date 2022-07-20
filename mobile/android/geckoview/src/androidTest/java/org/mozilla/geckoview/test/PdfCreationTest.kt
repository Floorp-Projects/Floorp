/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Color.rgb
import android.graphics.pdf.PdfRenderer
import android.os.ParcelFileDescriptor
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import junit.framework.TestCase.fail
import org.hamcrest.core.IsEqual.equalTo
import org.junit.Assert.assertTrue
import org.junit.After
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import java.io.File
import java.io.InputStream
import java.lang.Thread.sleep
import java.util.*
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
@LargeTest
class PdfCreationTest : BaseSessionTest() {
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    var deviceHeight = 0
    var deviceWidth = 0
    var scaledHeight = 0
    var scaledWidth = 12

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity {
            it.view.setSession(mainSession)
            deviceHeight = it.resources.displayMetrics.heightPixels
            deviceWidth = it.resources.displayMetrics.widthPixels
            scaledHeight = (scaledWidth * (deviceHeight / deviceWidth.toDouble())).roundToInt()
        }
    }

    @After
    fun cleanup() {
        activityRule.scenario.onActivity {
            it.view.releaseSession()
        }
    }

    private fun createFileDescriptor(pdfInputStream: InputStream): ParcelFileDescriptor {
        val file = File.createTempFile("temp", null);
        pdfInputStream.use { input ->
            file.outputStream().use { output ->
                input.copyTo(output)
            }
        }
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
    }

    private fun pdfToBitmap(pdfInputStream: InputStream): ArrayList<Bitmap>? {
        val bitmaps: ArrayList<Bitmap> = ArrayList()
        try {
            val pdfRenderer = PdfRenderer(createFileDescriptor(pdfInputStream))
            for (pageNo in 0 until pdfRenderer.pageCount) {
                val page = pdfRenderer.openPage(pageNo)
                var bitmap = Bitmap.createBitmap(deviceWidth, deviceHeight, Bitmap.Config.ARGB_8888)
                page.render(bitmap, null, null, PdfRenderer.Page.RENDER_MODE_FOR_DISPLAY)
                bitmaps.add(bitmap)
                page.close()
            }
            pdfRenderer.close()
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return bitmaps
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test fun singleColorPdf() {
        activityRule.scenario.onActivity {
            mainSession.loadTestPath(COLOR_ORANGE_BACKGROUND_HTML_PATH)
            mainSession.waitForPageStop()
            val pdfInputStream = mainSession.saveAsPdf()
            sessionRule.waitForResult(pdfInputStream).let {
                val bitmap = pdfToBitmap(it)!![0]
                val scaled = Bitmap.createScaledBitmap(bitmap, scaledWidth, scaledHeight, false)
                val centerPixel = scaled.getPixel(scaledWidth / 2, scaledHeight / 2)
                val orange = rgb(255, 113, 57)
                assertTrue("The PDF orange color matches.", centerPixel == orange)
            }
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test fun rgbColorsPdf() {
        activityRule.scenario.onActivity {
            mainSession.loadTestPath(COLOR_GRID_HTML_PATH)
            mainSession.waitForPageStop()
            val pdfInputStream = mainSession.saveAsPdf()
            sessionRule.waitForResult(pdfInputStream).let {
                val bitmap = pdfToBitmap(it)!![0]
                val scaled = Bitmap.createScaledBitmap(bitmap, scaledWidth, scaledHeight, false)
                val redPixel = scaled.getPixel(2,scaledHeight / 2)
                assertTrue("The PDF red color matches.", redPixel == Color.RED)
                val greenPixel = scaled.getPixel(scaledWidth / 2, scaledHeight / 2)
                assertTrue("The PDF green color matches.", greenPixel == Color.GREEN)
                val bluePixel = scaled.getPixel(scaledWidth - 2, scaledHeight / 2)
                assertTrue("The PDF blue color matches.", bluePixel == Color.BLUE)
                val doPixelsMatch = (redPixel == Color.RED
                        && greenPixel == Color.GREEN
                        && bluePixel == Color.BLUE)
                assertTrue("The PDF generated RGB colors.", doPixelsMatch)
            }
        }
    }

}
