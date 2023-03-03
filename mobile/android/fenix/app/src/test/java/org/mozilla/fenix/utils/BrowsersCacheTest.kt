/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.fenix.utils

import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageInfo
import android.content.pm.ResolveInfo
import android.net.Uri
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.Browsers
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
@Ignore("https://bugzilla.mozilla.org/show_bug.cgi?id=1794926")
class BrowsersCacheTest {

    // NB: There is always one more browser than pretendBrowsersAreInstalled installs because
    // the application we are testing is recognized as a browser itself!

    @Test
    fun `cached list of browsers match before-after installation when cache is not invalidated`() {
        BrowsersCache.resetAll()
        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
        )

        val initialBrowserList = BrowsersCache.all(testContext)
        assertEquals(3, initialBrowserList.installedBrowsers.size)

        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.FIREFOX.packageName,
                Browsers.KnownBrowser.CHROME.packageName,
                Browsers.KnownBrowser.SAMSUNG_INTERNET.packageName,
                Browsers.KnownBrowser.DUCKDUCKGO.packageName,
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
        )
        val updatedBrowserList = BrowsersCache.all(testContext)
        assertEquals(3, updatedBrowserList.installedBrowsers.size)
    }

    @Test
    fun `cached list of browsers change before-after installation when cache is invalidated`() {
        BrowsersCache.resetAll()
        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
        )

        val initialBrowserList = BrowsersCache.all(testContext)
        assertEquals(3, initialBrowserList.installedBrowsers.size)

        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.FIREFOX.packageName,
                Browsers.KnownBrowser.CHROME.packageName,
                Browsers.KnownBrowser.SAMSUNG_INTERNET.packageName,
                Browsers.KnownBrowser.DUCKDUCKGO.packageName,
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
        )

        BrowsersCache.resetAll()

        val updatedBrowserList = BrowsersCache.all(testContext)
        assertEquals(7, updatedBrowserList.installedBrowsers.size)
    }

    @Test
    fun `resetting the cache should empty it`() {
        BrowsersCache.resetAll()

        BrowsersCache.all(testContext)

        assertNotNull(BrowsersCache.cachedBrowsers)

        BrowsersCache.resetAll()

        assertNull(BrowsersCache.cachedBrowsers)
    }

    // pretendBrowsersAreInstalled was taken, verbatim, from a-c.
    // See support/utils/src/test/java/mozilla/components/support/utils/BrowsersTest.kt
    private fun pretendBrowsersAreInstalled(
        browsers: List<String> = listOf(),
        defaultBrowser: String? = null,
        url: String = "http://www.mozilla.org/index.html",
        browsersExported: Boolean = true,
        defaultBrowserExported: Boolean = true,
    ) {
        val packageManager = testContext.packageManager
        val shadow = shadowOf(packageManager)

        browsers.forEach { packageName ->
            val intent = Intent(Intent.ACTION_VIEW)
            intent.`package` = packageName
            intent.data = Uri.parse(url)
            intent.addCategory(Intent.CATEGORY_BROWSABLE)

            val packageInfo = PackageInfo().apply {
                this.packageName = packageName
            }

            shadow.installPackage(packageInfo)

            val activityInfo = ActivityInfo().apply {
                exported = browsersExported
                this.packageName = packageName
            }

            val resolveInfo = ResolveInfo().apply {
                resolvePackageName = packageName
                this.activityInfo = activityInfo
            }

            shadow.addResolveInfoForIntent(intent, resolveInfo)
        }

        if (defaultBrowser != null) {
            val intent = Intent(Intent.ACTION_VIEW)
            intent.data = Uri.parse(url)
            intent.addCategory(Intent.CATEGORY_BROWSABLE)

            val activityInfo = ActivityInfo().apply {
                exported = defaultBrowserExported
                packageName = defaultBrowser
            }

            val resolveInfo = ResolveInfo().apply {
                resolvePackageName = defaultBrowser
                this.activityInfo = activityInfo
            }

            shadow.addResolveInfoForIntent(intent, resolveInfo)
        }
    }
}
