/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageInfo
import android.content.pm.ResolveInfo
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.Browsers.Companion.SAMPLE_BROWSER_HTTP_URL
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows.shadowOf

@Suppress("DEPRECATION") // Deprecation will be handled in https://github.com/mozilla-mobile/android-components/issues/11832
@RunWith(AndroidJUnit4::class)
class BrowsersTest {

    @Test
    fun `with empty package manager`() {
        val browsers = Browsers.all(testContext)

        assertNull(browsers.defaultBrowser)
        assertNull(browsers.firefoxBrandedBrowser)
        assertFalse(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.installedBrowsers.isEmpty())
        assertFalse(browsers.hasThirdPartyDefaultBrowser)
        assertFalse(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `with firefox as default browser`() {
        pretendBrowsersAreInstalled(
            defaultBrowser = Browsers.KnownBrowser.FIREFOX.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertNotNull(browsers.defaultBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.defaultBrowser!!.packageName)

        assertNotNull(browsers.firefoxBrandedBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.firefoxBrandedBrowser!!.packageName)

        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)

        assertEquals(1, browsers.installedBrowsers.size)

        assertFalse(browsers.hasThirdPartyDefaultBrowser)
        assertFalse(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `with multiple browsers installed`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.FIREFOX.packageName,
                Browsers.KnownBrowser.CHROME.packageName,
                Browsers.KnownBrowser.SAMSUNG_INTERNET.packageName,
                Browsers.KnownBrowser.DUCKDUCKGO.packageName,
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
            defaultBrowser = Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertNotNull(browsers.defaultBrowser)
        assertEquals(Browsers.KnownBrowser.REFERENCE_BROWSER.packageName, browsers.defaultBrowser!!.packageName)

        assertNotNull(browsers.firefoxBrandedBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.firefoxBrandedBrowser!!.packageName)

        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)

        assertEquals(6, browsers.installedBrowsers.size)

        assertTrue(browsers.hasThirdPartyDefaultBrowser)
        assertTrue(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)

        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.REFERENCE_BROWSER))
        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.FIREFOX_NIGHTLY))
        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.FIREFOX))
        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.CHROME))
        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.REFERENCE_BROWSER))
        assertTrue(browsers.isInstalled(Browsers.KnownBrowser.DUCKDUCKGO))

        assertFalse(browsers.isInstalled(Browsers.KnownBrowser.CHROME_BETA))
        assertFalse(browsers.isInstalled(Browsers.KnownBrowser.FIREFOX_BETA))
        assertFalse(browsers.isInstalled(Browsers.KnownBrowser.ANDROID_STOCK_BROWSER))
        assertFalse(browsers.isInstalled(Browsers.KnownBrowser.UC_BROWSER))
    }

    @Test
    fun `With only firefox beta installed`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(Browsers.KnownBrowser.FIREFOX_BETA.packageName),
            defaultBrowser = Browsers.KnownBrowser.FIREFOX_BETA.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertEquals(Browsers.KnownBrowser.FIREFOX_BETA.packageName, browsers.defaultBrowser!!.packageName)
        assertEquals(Browsers.KnownBrowser.FIREFOX_BETA.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `With only firefox nightly installed`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName),
            defaultBrowser = Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertEquals(Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName, browsers.defaultBrowser!!.packageName)
        assertEquals(Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `With only firefox aurora installed`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(Browsers.KnownBrowser.FIREFOX_AURORA.packageName),
            defaultBrowser = Browsers.KnownBrowser.FIREFOX_AURORA.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertEquals(Browsers.KnownBrowser.FIREFOX_AURORA.packageName, browsers.defaultBrowser!!.packageName)
        assertEquals(Browsers.KnownBrowser.FIREFOX_AURORA.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `With only firefox froid installed`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(Browsers.KnownBrowser.FIREFOX_FDROID.packageName),
            defaultBrowser = Browsers.KnownBrowser.FIREFOX_FDROID.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertEquals(Browsers.KnownBrowser.FIREFOX_FDROID.packageName, browsers.defaultBrowser!!.packageName)
        assertEquals(Browsers.KnownBrowser.FIREFOX_FDROID.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `With this app being the default browser`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(testContext.packageName),
            defaultBrowser = testContext.packageName,
        )

        val browsers = Browsers.all(testContext)

        assertTrue(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)
        assertFalse(browsers.hasThirdPartyDefaultBrowser)
    }

    @Test
    fun `With unknown browsers`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(
                "org.example.random",
                "org.example.a.browser",
                Browsers.KnownBrowser.REFERENCE_BROWSER.packageName,
            ),
            defaultBrowser = "org.example.unknown.browser",
        )

        val browsers = Browsers.all(testContext)

        assertEquals("org.example.unknown.browser", browsers.defaultBrowser!!.packageName)
        assertNull(browsers.firefoxBrandedBrowser)
        assertFalse(browsers.hasFirefoxBrandedBrowserInstalled)
        assertEquals(2, browsers.installedBrowsers.size)
        assertTrue(browsers.hasThirdPartyDefaultBrowser)
        assertTrue(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)

        val installedBrowsers = browsers.installedBrowsers.map { it.packageName }
        assertTrue(installedBrowsers.contains("org.example.unknown.browser"))
        assertTrue(installedBrowsers.contains(Browsers.KnownBrowser.REFERENCE_BROWSER.packageName))
    }

    @Test
    fun `With default browser that is not exported`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName,
                Browsers.KnownBrowser.FIREFOX.packageName,
            ),
        )

        pretendBrowsersAreInstalled(
            defaultBrowser = "org.example.unknown.browser",
            defaultBrowserExported = false,
        )

        val browsers = Browsers.all(testContext)

        assertNull(browsers.defaultBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        browsers.installedBrowsers.forEach { println(it.packageName + " : " + it.exported) }
        assertEquals(2, browsers.installedBrowsers.size)
        assertFalse(browsers.hasThirdPartyDefaultBrowser)

        val installedBrowsers = browsers.installedBrowsers.map { it.packageName }
        assertTrue(installedBrowsers.contains(Browsers.KnownBrowser.FIREFOX.packageName))
        assertTrue(installedBrowsers.contains(Browsers.KnownBrowser.FIREFOX_NIGHTLY.packageName))
    }

    @Test
    fun `With some browsers not exported`() {
        pretendBrowsersAreInstalled(
            browsers = listOf(
                Browsers.KnownBrowser.FIREFOX.packageName,
            ),
        )

        pretendBrowsersAreInstalled(
            browsers = listOf(
                "org.example.area51.browser",
                Browsers.KnownBrowser.CHROME.packageName,
            ),
            browsersExported = false,
        )

        val browsers = Browsers.all(testContext)

        assertNull(browsers.defaultBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.firefoxBrandedBrowser!!.packageName)
        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)
        browsers.installedBrowsers.forEach { println(it.packageName + " : " + it.exported) }
        assertFalse(browsers.hasThirdPartyDefaultBrowser)
    }

    @Test
    fun `forUrl() with empty package manager`() {
        val browsers = Browsers.forUrl(testContext, SAMPLE_BROWSER_HTTP_URL)

        assertNull(browsers.defaultBrowser)
        assertNull(browsers.firefoxBrandedBrowser)
        assertFalse(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.installedBrowsers.isEmpty())
        assertFalse(browsers.hasThirdPartyDefaultBrowser)
        assertFalse(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `forUrl() with firefox as default browser`() {
        pretendBrowsersAreInstalled(
            defaultBrowser = Browsers.KnownBrowser.FIREFOX.packageName,
        )

        val browsers = Browsers.forUrl(testContext, SAMPLE_BROWSER_HTTP_URL)

        assertNotNull(browsers.defaultBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.defaultBrowser!!.packageName)

        assertNotNull(browsers.firefoxBrandedBrowser)
        assertEquals(Browsers.KnownBrowser.FIREFOX.packageName, browsers.firefoxBrandedBrowser!!.packageName)

        assertTrue(browsers.hasFirefoxBrandedBrowserInstalled)

        assertEquals(1, browsers.installedBrowsers.size)

        assertFalse(browsers.hasThirdPartyDefaultBrowser)
        assertFalse(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertTrue(browsers.isFirefoxDefaultBrowser)
    }

    @Test
    fun `forUrl() with wrong non-uri`() {
        pretendBrowsersAreInstalled(
            defaultBrowser = Browsers.KnownBrowser.FIREFOX.packageName,
        )

        val browsers = Browsers.forUrl(testContext, "not-a-uri")

        assertNull(browsers.defaultBrowser)
        assertNull(browsers.firefoxBrandedBrowser)
        assertFalse(browsers.hasFirefoxBrandedBrowserInstalled)
        assertTrue(browsers.installedBrowsers.isEmpty())
        assertFalse(browsers.hasThirdPartyDefaultBrowser)
        assertFalse(browsers.hasMultipleThirdPartyBrowsers)
        assertFalse(browsers.isDefaultBrowser)
        assertFalse(browsers.isFirefoxDefaultBrowser)
    }

    private fun pretendBrowsersAreInstalled(
        browsers: List<String> = listOf(),
        defaultBrowser: String? = null,
        url: String = SAMPLE_BROWSER_HTTP_URL,
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

            val packageInfo = PackageInfo()
            packageInfo.packageName = packageName

            shadow.installPackage(packageInfo)

            val activityInfo = ActivityInfo()
            activityInfo.exported = browsersExported
            activityInfo.packageName = packageName

            val resolveInfo = ResolveInfo()
            resolveInfo.resolvePackageName = packageName
            resolveInfo.activityInfo = activityInfo

            shadow.addResolveInfoForIntent(intent, resolveInfo)
        }

        if (defaultBrowser != null) {
            val intent = Intent(Intent.ACTION_VIEW)
            intent.data = Uri.parse(url)
            intent.addCategory(Intent.CATEGORY_BROWSABLE)

            val activityInfo = ActivityInfo()
            activityInfo.exported = defaultBrowserExported
            activityInfo.packageName = defaultBrowser

            val resolveInfo = ResolveInfo()
            resolveInfo.resolvePackageName = defaultBrowser
            resolveInfo.activityInfo = activityInfo

            shadow.addResolveInfoForIntent(intent, resolveInfo)
        }
    }
}
