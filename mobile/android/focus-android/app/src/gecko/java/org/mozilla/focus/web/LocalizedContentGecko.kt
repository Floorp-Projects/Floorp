/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web

import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.support.v4.util.ArrayMap
import android.view.View

import org.mozilla.focus.R
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.utils.HtmlLoader
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.geckoview.GeckoSession

import java.io.File
import java.io.PrintWriter

object LocalizedContentGecko {
    // We can't use "about:" because webview silently swallows about: pages, hence we use
    // a custom scheme.
    val URL_ABOUT = "focus:about"
    val URL_RIGHTS = "focus:rights"
    val mplUrl = "https://www.mozilla.org/en-US/MPL/"
    val trademarkPolicyUrl = "https://www.mozilla.org/foundation/trademarks/policy/"
    val gplUrl = "gpl.html"
    val trackingProtectionUrl = "https://wiki.mozilla.org/Security/Tracking_protection#Lists"
    val licensesUrl = "licenses.html"

    fun handleInternalContent(url: String, geckoSession: GeckoSession, context: Context): Boolean {
        if (URL_ABOUT == url) {
            loadAbout(geckoSession, context)
            return true
        } else if (URL_RIGHTS == url) {
            loadRights(geckoSession, context)
            return true
        }
        return false
    }

    /**
     * Load the content for focus:about
     */
    private fun loadAbout(geckoSession: GeckoSession, context: Context) {
        val resources = Locales.getLocalizedResources(context)

        val substitutionMap = ArrayMap<String, String>()
        val appName = context.resources.getString(R.string.app_name)
        val learnMoreURL = SupportUtils.getManifestoURL()

        var aboutVersion = ""
        try {
            val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
            aboutVersion = String.format("%s (Build #%s)", packageInfo.versionName, packageInfo.versionCode)
        } catch (e: PackageManager.NameNotFoundException) {
            // Nothing to do if we can't find the package name.
        }

        substitutionMap["%about-version%"] = aboutVersion

        val aboutContent = resources.getString(R.string.about_content, appName, learnMoreURL)
        substitutionMap["%about-content%"] = aboutContent

        val wordmark = HtmlLoader.loadPngAsDataURI(context, R.drawable.wordmark)
        substitutionMap["%wordmark%"] = wordmark

        putLayoutDirectionIntoMap(substitutionMap, context)

        val data = HtmlLoader.loadResourceFile(context, R.raw.about, substitutionMap)
        val path = context.filesDir
        val file = File(path, "about.html")
        writeDataToFile(file, data)

        geckoSession.loadUri(Uri.fromFile(file))
    }

    /**
     * Load the content for focus:rights
     */
    private fun loadRights(geckoSession: GeckoSession, context: Context) {
        val resources = Locales.getLocalizedResources(context)
        val appName = resources.getString(R.string.app_name)

        val substitutionMap = ArrayMap<String, String>()

        val content1 = resources.getString(R.string.your_rights_content1, appName)
        substitutionMap["%your-rights-content1%"] = content1

        val content2 = resources.getString(R.string.your_rights_content2, appName, mplUrl)
        substitutionMap["%your-rights-content2%"] = content2

        val content3 = resources.getString(R.string.your_rights_content3, appName, trademarkPolicyUrl)
        substitutionMap["%your-rights-content3%"] = content3

        val content4 = resources.getString(R.string.your_rights_content4, appName, licensesUrl)
        substitutionMap["%your-rights-content4%"] = content4

        val content5 = resources.getString(R.string.your_rights_content5, appName, gplUrl, trackingProtectionUrl)
        substitutionMap["%your-rights-content5%"] = content5

        putLayoutDirectionIntoMap(substitutionMap, context)

        val data = HtmlLoader.loadResourceFile(context, R.raw.rights, substitutionMap)
        val path = context.filesDir
        val file = File(path, "rights.html")

        writeDataToFile(file, data)

        geckoSession.loadUri(Uri.fromFile(file))
    }

    private fun writeDataToFile(file: File, data: String) {
        PrintWriter(file).use({ out -> out.println(data) })
    }

    private fun putLayoutDirectionIntoMap(substitutionMap: ArrayMap<String, String>, context: Context) {
        val layoutDirection = context.resources.configuration.layoutDirection

        val direction = when (layoutDirection) {
            View.LAYOUT_DIRECTION_LTR -> "ltr"
            View.LAYOUT_DIRECTION_RTL -> "rtl"
            else -> "auto"
        }

        substitutionMap["%dir%"] = direction
    }
}
