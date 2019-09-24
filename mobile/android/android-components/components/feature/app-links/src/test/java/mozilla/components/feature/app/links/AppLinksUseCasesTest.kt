/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class AppLinksUseCasesTest {

    private val appUrl = "https://example.com"
    private val appPackage = "com.example.app"
    private val browserPackage = "com.browser"

    private fun createContext(vararg urlToPackages: Pair<String, String>): Context {
        val pm = testContext.packageManager
        val packageManager = shadowOf(pm)

        urlToPackages.forEach { (urlString, packageName) ->
            val intent = Intent.parseUri(urlString, 0).addCategory(Intent.CATEGORY_BROWSABLE)

            val activityInfo = ActivityInfo()
            activityInfo.packageName = packageName

            val resolveInfo = ResolveInfo()
            resolveInfo.activityInfo = activityInfo

            packageManager.addResolveInfoForIntentNoDefaults(intent, resolveInfo)
        }

        val context = mock<Context>()
        `when`(context.packageManager).thenReturn(pm)

        return context
    }

    @Test
    fun `A URL that matches zero apps is not an app link`() {
        val context = createContext()
        val subject = AppLinksUseCases(context, emptySet())

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A web URL that matches more than zero apps is an app link`() {
        val context = createContext(appUrl to appPackage)
        val subject = AppLinksUseCases(context, emptySet())

        // We will not redirect to it when we click on it.
        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())

        // But we do from a context menu.
        val menuRedirect = subject.appLinkRedirect(appUrl)
        assertTrue(menuRedirect.isRedirect())
    }

    @Test
    fun `A URL that matches only excluded packages is not an app link`() {
        val context = createContext(appUrl to browserPackage)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        val redirect = subject.interceptedAppLinkRedirect(appUrl)
        assertFalse(redirect.isRedirect())
    }

    @Test
    fun `A URL that also matches excluded packages is an app link`() {
        val context = createContext(appUrl to appPackage, appUrl to browserPackage)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        val redirect = subject.appLinkRedirect(appUrl)
        assertTrue(redirect.isRedirect())
    }

    @Test
    fun `A list of browser package names can be generated if not supplied`() {
        val unguessable = "https://unguessable-test-url.com"
        val context = createContext(unguessable to browserPackage)
        val subject = AppLinksUseCases(context, unguessableWebUrl = unguessable)

        assertEquals(subject.browserPackageNames, setOf(browserPackage))
    }

    @Test
    fun `A intent scheme uri with an installed app`() {
        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;end"
        val context = createContext(uri to appPackage, appUrl to browserPackage)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertTrue(redirect.hasExternalApp())
        assertNotNull(redirect.appIntent)

        assertEquals("zxing://scan/", redirect.appIntent!!.dataString)
    }

    @Test
    fun `A intent scheme uri without an installed app`() {
        val context = createContext(appUrl to browserPackage)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;end"

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertFalse(redirect.hasFallback())
        assertNull(redirect.webUrl)
    }

    @Test
    fun `A intent scheme uri with a fallback, but without an installed app`() {
        val context = createContext(appUrl to browserPackage)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        val uri = "intent://scan/#Intent;scheme=zxing;package=com.google.zxing.client.android;S.browser_fallback_url=http%3A%2F%2Fzxing.org;end"

        val redirect = subject.interceptedAppLinkRedirect(uri)
        assertFalse(redirect.hasExternalApp())
        assertTrue(redirect.hasFallback())

        assertEquals("http://zxing.org", redirect.webUrl)
    }

    @Test
    fun `An openAppLink use case starts an activity`() {
        val context = createContext()
        val appIntent = Intent()
        val redirect = AppLinkRedirect(appIntent, appUrl, false)
        val subject = AppLinksUseCases(context, setOf(browserPackage))

        subject.openAppLink(redirect)

        verify(context).startActivity(any())
    }
}
