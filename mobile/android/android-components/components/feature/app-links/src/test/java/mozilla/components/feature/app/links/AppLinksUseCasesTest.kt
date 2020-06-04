/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf
import java.io.File

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AppLinksUseCasesTest {

    private val appUrl = "https://example.com"
    private val appIntent = "intent://example.com"
    private val appSchemeIntent = "example://example.com"
    private val appPackage = "com.example.app"
    private val browserUrl = "browser://test"
    private val browserPackage = "com.browser"
    private val testBrowserPackage = "com.current.browser"
    private val filePath = "file:///storage/abc/test.mp3"
    private val dataUrl = "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="
    private val aboutUrl = "about:config"
    private val javascriptUrl = "javascript:'hello, world'"
    private val fileType = "audio/mpeg"

    @Before
    fun setup() {
        AppLinksUseCases.redirectCache = null
        AppLinksUseCases.browserNamesCache = null
    }

    private fun createContext(vararg urlToPackages: Pair<String, String>, default: Boolean = false): Context {

        val pm = testContext.packageManager
        val packageManager = shadowOf(pm)

        urlToPackages.forEach { (urlString, pkgName) ->
            val intent = Intent.parseUri(urlString, 0).addCategory(Intent.CATEGORY_BROWSABLE)

            val info = ActivityInfo().apply {
                packageName = pkgName
                icon = android.R.drawable.btn_default
            }

            val resolveInfo = ResolveInfo().apply {
                labelRes = android.R.string.ok
                activityInfo = info
            }
            packageManager.addResolveInfoForIntent(intent, resolveInfo)
            packageManager.addDrawableResolution(pkgName, android.R.drawable.btn_default, mock())
        }

        val context = mock<Context>()
        `when`(context.packageManager).thenReturn(pm)
        if (!default) {
            `when`(context.packageName).thenReturn(testBrowserPackage)
        }

        return context
    }

    @Test
    fun `A malformed URL should not cause a crash`() {
        val context = createContext()
        val subject = AppLinksUseCases(context, { true })
        val redirect = subject.interceptedAppLinkRedirect("test://test#Intent;")
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A URL that matches zero apps is not an app link`() {
        val context = createContext()
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A web URL that matches more than zero apps is an app link`() {
        val context = createContext(appUrl to appPackage)
        val subject = AppLinksUseCases(context, { true })

        // We will redirect to it if browser option set to true.
        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertTrue(redirect.isRedirect())
    }

    @Test
    fun `A file is not an app link`() {
        val context = createContext(filePath to appPackage)
        val subject = AppLinksUseCases(context, { true })

        // We will redirect to it if browser option set to true.
        val redirect = subject.interceptedAppLinkRedirect(filePath)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A data url is not an app link`() {
        val context = createContext(dataUrl to appPackage)
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(dataUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A javascript url is not an app link`() {
        val context = createContext(javascriptUrl to appPackage)
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(javascriptUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `An about url is not an app link`() {
        val context = createContext(aboutUrl to appPackage)
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(aboutUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `Will not redirect app link if browser option set to false and scheme is supported`() {
        val context = createContext(appUrl to appPackage)
        val subject = AppLinksUseCases(context, { false })

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())

        val menuRedirect = subject.appLinkRedirect(appUrl)
        assertTrue(menuRedirect.isRedirect())
    }

    @Test
    fun `Will redirect app link if browser option set to false and scheme is not supported`() {
        val context = createContext(appIntent to appPackage)
        val subject = AppLinksUseCases(context, { false })

        val redirect = subject.interceptedAppLinkRedirect(appIntent)
        assertTrue(redirect.isRedirect())

        val menuRedirect = subject.appLinkRedirect(appIntent)
        assertTrue(menuRedirect.isRedirect())
    }

    @Test
    fun `A URL that matches only excluded packages is not an app link`() {
        val context = createContext(appUrl to browserPackage, browserUrl to browserPackage)
        val subject = AppLinksUseCases(context, { true }, unguessableWebUrl = browserUrl)

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())

        val menuRedirect = subject.appLinkRedirect(appUrl)
        assertFalse(menuRedirect.hasExternalApp())
    }

    @Test
    fun `A URL that also matches excluded packages is an app link`() {
        val context = createContext(appUrl to appPackage, appUrl to browserPackage, browserUrl to browserPackage)
        val subject = AppLinksUseCases(context, { true }, unguessableWebUrl = browserUrl)

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertTrue(redirect.isRedirect())

        // But we do from a context menu.
        val menuRedirect = subject.appLinkRedirect(appUrl)
        assertTrue(menuRedirect.isRedirect())
    }

    @Test
    fun `A URL that only matches default activity is not an app link`() {
        val context = createContext(appUrl to appPackage, default = true)
        val subject = AppLinksUseCases(context, { true })

        val menuRedirect = subject.appLinkRedirect(appUrl)
        assertFalse(menuRedirect.hasExternalApp())
    }

    @Test
    fun `A list of browser package names can be generated if not supplied`() {
        val unguessable = "https://unguessable-test-url.com"
        val context = createContext(unguessable to browserPackage)
        val subject = AppLinksUseCases(context, unguessableWebUrl = unguessable)

        subject.appLinkRedirect(unguessable)
        assertEquals(subject.getBrowserPackageNames(), setOf(browserPackage))
    }

    @Test
    fun `A intent scheme uri with an installed app is an app link`() {
        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;end"
        val context = createContext(uri to appPackage)
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertTrue(redirect.hasExternalApp())
        assertNotNull(redirect.appIntent)
        assertNotNull(redirect.marketplaceIntent)

        assertEquals("zxing://scan/", redirect.appIntent!!.dataString)
    }

    @Test
    fun `A market scheme uri with no installed app is an install link`() {
        val uri = "intent://details/#Intent;scheme=market;package=com.google.play;end"
        val context = createContext(uri to appPackage)
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect.invoke(uri)

        assertTrue(redirect.hasExternalApp())
        assertTrue(redirect.isInstallable())
        assert(redirect.marketplaceIntent!!.flags and Intent.FLAG_ACTIVITY_NEW_TASK
            == Intent.FLAG_ACTIVITY_NEW_TASK)
    }

    @Test
    fun `A intent scheme uri without an installed app is not an app link`() {
        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;end"
        val context = createContext()
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.fallbackUrl)
        assertFalse(redirect.isInstallable())
    }

    @Test
    fun `A intent scheme uri with a fallback without an installed app is not an app link`() {
        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;S.browser_fallback_url=http%3A%2F%2Fzxing.org;end"
        val context = createContext()
        val subject = AppLinksUseCases(context, { true })

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertTrue(redirect.hasFallback())

        assertEquals("http://zxing.org", redirect.fallbackUrl)
    }

    @Test
    fun `A intent scheme denied should return no app intent`() {
        val uri = "intent://details/#Intent"
        val context = createContext(uri to appPackage)
        val subject = AppLinksUseCases(context, { true }, alwaysDeniedSchemes = setOf("intent"))

        val redirect = subject.interceptedAppLinkRedirect.invoke(uri)

        assertNull(redirect.appIntent)
        assertFalse(redirect.hasExternalApp())
    }

    @Test
    fun `An openAppLink use case starts an activity`() {
        val context = createContext()
        val appIntent = Intent()
        val redirect = AppLinkRedirect(appIntent, appUrl, null)
        val subject = AppLinksUseCases(context, { true })

        subject.openAppLink(redirect.appIntent)

        verify(context).startActivity(any())
    }

    @Test
    fun `Start activity fails will perform failure action`() {
        val context = createContext()
        val appIntent = Intent()
        val redirect = AppLinkRedirect(appIntent, appUrl, null)
        val subject = AppLinksUseCases(context, { true })

        var failedToLaunch = false
        val failedAction = { failedToLaunch = true }
        `when`(context.startActivity(any())).thenThrow(ActivityNotFoundException("failed"))
        subject.openAppLink(redirect.appIntent, failedToLaunchAction = failedAction)

        verify(context).startActivity(any())
        assert(failedToLaunch)
    }

    @Test
    fun `AppLinksUsecases uses cache`() {
        val testDispatcher = TestCoroutineDispatcher()
        TestCoroutineScope(testDispatcher).launch {
            val context = createContext(appUrl to appPackage)

            var subject = AppLinksUseCases(context, { true })
            var redirect = subject.interceptedAppLinkRedirect(appUrl)
            assertTrue(redirect.isRedirect())
            val timestamp = AppLinksUseCases.redirectCache?.cacheTimeStamp

            testDispatcher.advanceTimeBy(APP_LINKS_CACHE_INTERVAL / 2)
            subject = AppLinksUseCases(context, { true })
            redirect = subject.interceptedAppLinkRedirect(appUrl)
            assertTrue(redirect.isRedirect())
            assert(timestamp == AppLinksUseCases.redirectCache?.cacheTimeStamp)

            testDispatcher.advanceTimeBy(APP_LINKS_CACHE_INTERVAL / 2 + 1)
            subject = AppLinksUseCases(context, { true })
            redirect = subject.interceptedAppLinkRedirect(appUrl)
            assertTrue(redirect.isRedirect())
            assert(timestamp != AppLinksUseCases.redirectCache?.cacheTimeStamp)
        }
    }

    @Test
    fun `AppLinksUsecases uses browser names cache`() {
        val testDispatcher = TestCoroutineDispatcher()
        TestCoroutineScope(testDispatcher).launch {
            val context = createContext(appUrl to appPackage)

            var subject = AppLinksUseCases(context, { true })
            whenever(subject.findExcludedPackages(any())).thenReturn(emptySet())
            var browserNames = subject.getBrowserPackageNames()
            assertTrue(browserNames.isEmpty())
            val timestamp = AppLinksUseCases.browserNamesCache?.cacheTimeStamp

            whenever(subject.findExcludedPackages(any())).thenReturn(setOf(appPackage))
            testDispatcher.advanceTimeBy(APP_LINKS_CACHE_INTERVAL / 2)
            subject = AppLinksUseCases(context, { true })
            browserNames = subject.getBrowserPackageNames()
            assertTrue(browserNames.isEmpty())
            assert(timestamp == AppLinksUseCases.browserNamesCache?.cacheTimeStamp)

            testDispatcher.advanceTimeBy(APP_LINKS_CACHE_INTERVAL / 2 + 1)
            subject = AppLinksUseCases(context, { true })
            browserNames = subject.getBrowserPackageNames()
            assertFalse(browserNames.isEmpty())
            assertFalse(browserNames.contains(appPackage))
            assert(timestamp != AppLinksUseCases.browserNamesCache?.cacheTimeStamp)
        }
    }

    @Test
    fun `OpenAppLinkRedirect should not try to open files`() {
        val context = spy(createContext())
        val uri = Uri.fromFile(File(filePath))
        val intent = Intent(Intent.ACTION_VIEW)
        intent.setDataAndType(uri, fileType)
        val subject = AppLinksUseCases(context, { true })

        subject.openAppLink(intent)

        verify(context, never()).startActivity(any())
    }

    @Test
    fun `OpenAppLinkRedirect should not try to open data URIs`() {
        val context = spy(createContext())
        val uri = Uri.parse(dataUrl)
        val intent = Intent(Intent.ACTION_VIEW)
        intent.setDataAndType(uri, fileType)
        val subject = AppLinksUseCases(context, { true })

        subject.openAppLink(intent)

        verify(context, never()).startActivity(any())
    }

    @Test
    fun `OpenAppLinkRedirect should not try to open javascript URIs`() {
        val context = spy(createContext())
        val uri = Uri.parse(javascriptUrl)
        val intent = Intent(Intent.ACTION_VIEW)
        intent.setDataAndType(uri, fileType)
        val subject = AppLinksUseCases(context, { true })

        subject.openAppLink(intent)

        verify(context, never()).startActivity(any())
    }

    @Test
    fun `OpenAppLinkRedirect should not try to open about URIs`() {
        val context = spy(createContext())
        val uri = Uri.parse(aboutUrl)
        val intent = Intent(Intent.ACTION_VIEW)
        intent.setDataAndType(uri, fileType)
        val subject = AppLinksUseCases(context, { true })

        subject.openAppLink(intent)

        verify(context, never()).startActivity(any())
    }

    @Test
    fun `A intent scheme uri with package name will have marketplace intent regardless user preference`() {
        val context = createContext()
        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;end"
        var subject = AppLinksUseCases(context, { false })
        var redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNotNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)

        subject = AppLinksUseCases(context, { true })
        redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNotNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)
    }

    @Test
    fun `A app scheme uri will redirect regardless user preference`() {
        val context = createContext(appSchemeIntent to appPackage)

        var subject = AppLinksUseCases(context, { false })
        var redirect = subject.interceptedAppLinkRedirect(appSchemeIntent)
        assertTrue(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)
        assertTrue(redirect.appIntent?.flags?.and(Intent.FLAG_ACTIVITY_CLEAR_TASK) == 0)

        subject = AppLinksUseCases(context, { true })
        redirect = subject.interceptedAppLinkRedirect(appSchemeIntent)
        assertTrue(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)
        assertTrue(redirect.appIntent?.flags?.and(Intent.FLAG_ACTIVITY_CLEAR_TASK) == 0)
    }

    @Test
    fun `A app intent uri will redirect regardless user preference`() {
        val context = createContext(appIntent to appPackage)

        var subject = AppLinksUseCases(context, { false })
        var redirect = subject.interceptedAppLinkRedirect(appIntent)
        assertTrue(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)
        assertTrue(redirect.appIntent?.flags?.and(Intent.FLAG_ACTIVITY_CLEAR_TASK) == 0)

        subject = AppLinksUseCases(context, { true })
        redirect = subject.interceptedAppLinkRedirect(appIntent)
        assertTrue(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.marketplaceIntent)
        assertNull(redirect.fallbackUrl)
        assertTrue(redirect.appIntent?.flags?.and(Intent.FLAG_ACTIVITY_CLEAR_TASK) == 0)
    }
}
