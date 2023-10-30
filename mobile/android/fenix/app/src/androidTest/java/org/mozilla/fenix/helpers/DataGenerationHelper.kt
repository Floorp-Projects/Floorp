/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiSelector
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.availableSearchEngines
import org.junit.Assert
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.utils.IntentUtils

object DataGenerationHelper {
    val appContext: Context = InstrumentationRegistry.getInstrumentation().targetContext

    fun createCustomTabIntent(
        pageUrl: String,
        customMenuItemLabel: String = "",
        customActionButtonDescription: String = "",
    ): Intent {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), IntentUtils.defaultIntentPendingFlags)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(customMenuItemLabel, pendingIntent)
            .setShareState(CustomTabsIntent.SHARE_STATE_ON)
            .setActionButton(
                createTestBitmap(),
                customActionButtonDescription,
                pendingIntent,
                true,
            )
            .build()
        customTabsIntent.intent.data = Uri.parse(pageUrl)
        return customTabsIntent.intent
    }

    private fun createTestBitmap(): Bitmap {
        val bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        return bitmap
    }

    fun getStringResource(id: Int, argument: String = TestHelper.appName) = TestHelper.appContext.resources.getString(id, argument)

    private val charPool: List<Char> = ('a'..'z') + ('A'..'Z') + ('0'..'9')
    fun generateRandomString(stringLength: Int) =
        (1..stringLength)
            .map { kotlin.random.Random.nextInt(0, charPool.size) }
            .map(charPool::get)
            .joinToString("")

    /**
     * Creates clipboard data.
     */
    fun setTextToClipBoard(context: Context, message: String) {
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clipData = ClipData.newPlainText("label", message)

        clipBoard.setPrimaryClip(clipData)
    }

    /**
     * Returns sponsored shortcut title based on the index.
     */
    fun getSponsoredShortcutTitle(position: Int): String {
        val sponsoredShortcut = mDevice.findObject(
            UiSelector()
                .resourceId("${TestHelper.packageName}:id/top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .resourceId("${TestHelper.packageName}:id/top_site_title"),
        ).text

        return sponsoredShortcut
    }

    /**
     * The list of Search engines for the "home" region of the user.
     * For en-us it will return the 6 engines selected by default: Google, Bing, DuckDuckGo, Amazon, Ebay, Wikipedia.
     */
    fun getRegionSearchEnginesList(): List<SearchEngine> {
        val searchEnginesList = appContext.components.core.store.state.search.regionSearchEngines
        Assert.assertTrue("Search engines list returned nothing", searchEnginesList.isNotEmpty())
        return searchEnginesList
    }

    /**
     * The list of Search engines available to be added by user choice.
     * For en-us it will return the 2 engines: Reddit, Youtube.
     */
    fun getAvailableSearchEngines(): List<SearchEngine> {
        val searchEnginesList = TestHelper.appContext.components.core.store.state.search.availableSearchEngines
        Assert.assertTrue("Search engines list returned nothing", searchEnginesList.isNotEmpty())
        return searchEnginesList
    }
}
