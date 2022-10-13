/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.net.Uri
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import java.util.HashMap

/**
 * Helpful tools for dealing with other browsers on this device.
 *
 * ```
 * // Collect information about all installed browsers:
 * val browsers = Browsers.all(context)
 *
 * // Collect information about installed browsers (and apps) that can handle a specific URL:
 * val browsers = Browsers.forUrl(context, url)`
 * ```
 */
@SuppressLint("QueryPermissionsNeeded") // Yes, this class needs the permission to read all packages
class Browsers private constructor(
    context: Context,
    uri: Uri,
) {
    /**
     * Enum of known browsers and their package names.
     */
    enum class KnownBrowser constructor(
        val packageName: String,
    ) {
        FIREFOX("org.mozilla.firefox"),

        FIREFOX_BETA("org.mozilla.firefox_beta"),
        FIREFOX_AURORA("org.mozilla.fennec_aurora"),
        FIREFOX_FENNEC_NIGHTLY("org.mozilla.fennec"),
        FIREFOX_FDROID("org.mozilla.fennec_fdroid"),

        FIREFOX_LITE("org.mozilla.rocket"),

        FIREFOX_NIGHTLY("org.mozilla.fenix"),
        FENIX_DEBUG("org.mozilla.fenix.debug"),

        FIREFOX_FOCUS_DEBUG("org.mozilla.focus.debug"),
        FIREFOX_FOCUS_NIGHTLY("org.mozilla.focus.nightly"),
        FIREFOX_FOCUS_BETA("org.mozilla.focus.beta"),
        FIREFOX_FOCUS("org.mozilla.focus"),

        REFERENCE_BROWSER("org.mozilla.reference.browser"),
        REFERENCE_BROWSER_DEBUG("org.mozilla.reference.browser.debug"),

        CHROME("com.android.chrome"),
        CHROME_BETA("com.chrome.beta"),
        CHROME_DEV("com.chrome.dev"),
        CHROME_CANARY("com.chrome.canary"),
        CHROME_LOCAL_BUILD("com.google.android.apps.chrome"),
        CHROMIUM_LOCAL_BUILD("org.chromium.chrome"),

        OPERA("com.opera.browser"),
        OPERA_BETA("com.opera.browser.beta"),
        OPERA_MINI("com.opera.mini.native"),
        OPERA_MINI_BETA("com.opera.mini.native.beta"),

        UC_BROWSER("com.UCMobile.intl"),
        UC_BROWSER_MINI("com.uc.browser.en"),

        ANDROID_STOCK_BROWSER("com.android.browser"),

        SAMSUNG_INTERNET("com.sec.android.app.sbrowser"),
        SAMSUNG_INTERNET_BETA("com.sec.android.app.sbrowser.beta"),

        ORFOX("info.guardianproject.orfox"),
        TOR_BROWSER_ALPHA("org.torproject.torbrowser_alpha"),

        MICROSOFT_EDGE("com.microsoft.emmx"),
        DOLPHIN_BROWSER("mobi.mgeek.TunnyBrowser"),
        BRAVE_BROWSER("com.brave.browser"),
        LINK_BUBBLE("com.linkbubble.playstore"),
        ADBLOCK_BROWSER("org.adblockplus.browser"),
        CHROMER("arun.com.chromer"),
        FLYNX("com.flynx"),
        GHOSTERY_BROWSER("com.ghostery.android.ghostery"),
        DUCKDUCKGO("com.duckduckgo.mobile.android"),
        CLIQZ("com.cliqz.browser"),
    }

    private val packageName = context.packageName

    private val browsers: Map<String, ActivityInfo> = {
        val packageManager = context.packageManager

        val browsers = resolveBrowsers(context, packageManager, uri)

        // If there's a default browser set then modern Android systems won't return other browsers
        // anymore when using queryIntentActivities(). That's annoying and our only option is
        // to go through a list of known browsers and see if anyone of them is installed and
        // wants to handle our URL.
        findKnownBrowsers(packageManager, browsers, uri)

        browsers
    }()

    /**
     * The [ActivityInfo] of the default browser of the user (or null if none could be found).
     */
    val defaultBrowser: ActivityInfo? = findDefault(context, context.packageManager, uri)

    /**
     * The [ActivityInfo] of the installed Firefox browser (or null if none could be found).
     *
     * If multiple Firefox browsers are installed then this will
     */
    val firefoxBrandedBrowser: ActivityInfo? = findFirefoxBrandedBrowser()

    /**
     * Is there a Firefox browser installed on this device?
     */
    val hasFirefoxBrandedBrowserInstalled: Boolean = firefoxBrandedBrowser != null

    /**
     * Is Firefox (Release, Beta, Nightly) the default browser of the user?
     */
    val isFirefoxDefaultBrowser: Boolean
        get() =
            defaultBrowser != null && (
                defaultBrowser.packageName == KnownBrowser.FIREFOX.packageName ||
                    defaultBrowser.packageName == KnownBrowser.FIREFOX_BETA.packageName ||
                    defaultBrowser.packageName == KnownBrowser.FIREFOX_AURORA.packageName ||
                    defaultBrowser.packageName == KnownBrowser.FIREFOX_FENNEC_NIGHTLY.packageName ||
                    defaultBrowser.packageName == KnownBrowser.FIREFOX_NIGHTLY.packageName ||
                    defaultBrowser.packageName == KnownBrowser.FIREFOX_FDROID.packageName
                )

    /**
     * List of [ActivityInfo] of all known installed browsers.
     */
    val installedBrowsers: List<ActivityInfo> = browsers.values.toList()

    /**
     * Does this device have a default browser that is not Firefox (release) or **this** app calling the method.
     */
    val hasThirdPartyDefaultBrowser: Boolean = (
        defaultBrowser != null &&
            defaultBrowser.packageName != KnownBrowser.FIREFOX.packageName &&
            !(firefoxBrandedBrowser != null && defaultBrowser.packageName == firefoxBrandedBrowser.packageName) &&
            defaultBrowser.packageName != packageName
        )

    /**
     * Does this device have multiple third-party browser installed?
     */
    val hasMultipleThirdPartyBrowsers: Boolean
        get() {
            if (browsers.size > 1) {
                // There are more than us and Firefox.
                return true
            }

            for (info in browsers.values) {
                if (info !== defaultBrowser &&
                    info.packageName != KnownBrowser.FIREFOX.packageName &&
                    info.packageName != packageName
                ) {
                    // There's at least one browser that is not *this app* or Firefox and also not the
                    // default browser.
                    return true
                }
            }

            return false
        }

    /**
     * Does this device have [browser] installed?
     */
    fun isInstalled(browser: KnownBrowser): Boolean {
        return browsers.containsKey(browser.packageName)
    }

    /**
     * Is **this** application the default browser?
     */
    val isDefaultBrowser: Boolean = defaultBrowser != null && packageName == defaultBrowser.packageName

    private fun findFirefoxBrandedBrowser(): ActivityInfo? {
        return when {
            browsers.containsKey(KnownBrowser.FIREFOX.packageName) ->
                browsers[KnownBrowser.FIREFOX.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_BETA.packageName) ->
                browsers[KnownBrowser.FIREFOX_BETA.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_AURORA.packageName) ->
                browsers[KnownBrowser.FIREFOX_AURORA.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_NIGHTLY.packageName) ->
                browsers[KnownBrowser.FIREFOX_NIGHTLY.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_FDROID.packageName) ->
                browsers[KnownBrowser.FIREFOX_FDROID.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_FOCUS.packageName) ->
                browsers[KnownBrowser.FIREFOX_FOCUS.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_FOCUS_DEBUG.packageName) ->
                browsers[KnownBrowser.FIREFOX_FOCUS_DEBUG.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_FOCUS_BETA.packageName) ->
                browsers[KnownBrowser.FIREFOX_FOCUS_BETA.packageName]

            browsers.containsKey(KnownBrowser.FIREFOX_FOCUS_NIGHTLY.packageName) ->
                browsers[KnownBrowser.FIREFOX_FOCUS_NIGHTLY.packageName]
            else -> null
        }
    }

    private fun resolveBrowsers(
        context: Context,
        packageManager: PackageManager,
        uri: Uri,
    ): MutableMap<String, ActivityInfo> {
        val browsers = HashMap<String, ActivityInfo>()
        val resolvers = findResolvers(context, packageManager, includeThisApp = false, url = uri.toString())

        for (info in resolvers) {
            browsers[info.activityInfo.packageName] = info.activityInfo
        }

        return browsers
    }

    private fun findKnownBrowsers(
        packageManager: PackageManager,
        browsers: MutableMap<String, ActivityInfo>,
        uri: Uri,
    ) {
        for (browser in KnownBrowser.values()) {
            if (browsers.containsKey(browser.packageName)) {
                continue
            }

            // resolveActivity() can be slow if the package isn't installed (e.g. 200ms on an N6 with a bad WiFi
            // connection). Hence we query if the package is installed first, and only call resolveActivity for
            // installed packages. getPackageInfo() is fast regardless of a package being installed
            try {
                // We don't need the result, we only need to detect when the package doesn't exist
                packageManager.getPackageInfo(browser.packageName, 0)
            } catch (e: PackageManager.NameNotFoundException) {
                continue
            }

            val intent = Intent(Intent.ACTION_VIEW)
            intent.data = uri
            intent.setPackage(browser.packageName)
            intent.addCategory(Intent.CATEGORY_BROWSABLE)

            val info = packageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY)
                ?: continue

            if (info.activityInfo == null || !info.activityInfo.exported) {
                continue
            }

            browsers[info.activityInfo.packageName] = info.activityInfo
        }
    }

    private fun findDefault(context: Context, packageManager: PackageManager, uri: Uri): ActivityInfo? {
        val intent = Intent(Intent.ACTION_VIEW, uri)
        intent.addCategory(Intent.CATEGORY_BROWSABLE)

        val resolveInfo = packageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY)
            ?: return null

        if (resolveInfo.activityInfo == null || !resolveInfo.activityInfo.exported) {
            // We are not allowed to launch this activity.
            return null
        }

        return if (!browsers.containsKey(resolveInfo.activityInfo.packageName) &&
            resolveInfo.activityInfo.packageName != context.packageName
        ) {
            // This default browser wasn't returned when we asked for *all* browsers. It's likely
            // that this is actually the resolver activity (aka intent chooser). Let's ignore it.
            null
        } else {
            resolveInfo.activityInfo
        }
    }

    companion object {
        @VisibleForTesting
        internal const val SAMPLE_BROWSER_HTTP_URL = "http://www.mozilla.org/index.html"
        private const val SAMPLE_BROWSER_HTTPS_URL = "https://www.mozilla.org/index.html"

        // Sample URL handled by traditional web browsers. Used to find installed (basic) web browsers.
        private val SAMPLE_BROWSER_URI = Uri.parse(SAMPLE_BROWSER_HTTP_URL)

        /**
         * Returns `true` is the provided [packageName] matches a known browser.
         */
        fun isBrowser(packageName: String): Boolean {
            return KnownBrowser.values().asSequence().firstOrNull { browser ->
                browser.packageName == packageName
            } != null
        }

        /**
         * Collect information about all installed browsers and return a [Browsers] object containing that data.
         */
        fun all(context: Context): Browsers = Browsers(context, SAMPLE_BROWSER_URI)

        /**
         * Collect information about all installed browsers that can handle the specified URL and return a [Browsers]
         * object containing that data.
         */
        fun forUrl(context: Context, url: String) = Browsers(context, Uri.parse(url))

        /**
         * Finds all the [ResolveInfo] for the installed browsers.
         * @return A list of all [ResolveInfo] for the installed browsers.
         */
        fun findResolvers(
            context: Context,
            packageManager: PackageManager,
            includeThisApp: Boolean = true,
        ): List<ResolveInfo> {
            val httpIntent = Intent.parseUri(SAMPLE_BROWSER_HTTP_URL, Intent.URI_INTENT_SCHEME)
            val httpsIntent = Intent.parseUri(SAMPLE_BROWSER_HTTPS_URL, Intent.URI_INTENT_SCHEME)

            val flag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                PackageManager.MATCH_ALL
            } else {
                PackageManager.MATCH_DEFAULT_ONLY
            }

            val httpResults = packageManager.queryIntentActivities(httpIntent, flag)
                .orEmpty()
                .filter {
                    it.activityInfo.exported &&
                        (includeThisApp || it.activityInfo.packageName != context.packageName)
                }

            val httpsResults = packageManager.queryIntentActivities(httpsIntent, flag)
                .orEmpty()
                .filter {
                    it.activityInfo.exported &&
                        (includeThisApp || it.activityInfo.packageName != context.packageName)
                }

            // There apps that have the same activityInfo.name to make it unique we
            // combine the activityInfo.packageName + activityInfo.name
            return (httpResults + httpsResults).distinctBy { it.activityInfo.packageName + it.activityInfo.name }
        }

        /**
         * Finds all the [ResolveInfo] for the installed browsers that can handle the specified URL [url].
         * @return A list of all [ResolveInfo] that correspond to the given [url].
         */
        fun findResolvers(
            context: Context,
            packageManager: PackageManager,
            url: String,
            includeThisApp: Boolean = true,
            contentType: String? = null,
        ): List<ResolveInfo> {
            val uri = url.toUri()
            val intent = Intent(Intent.ACTION_VIEW).apply {
                if (contentType != null) setDataAndTypeAndNormalize(uri, contentType) else data = uri
                addCategory(Intent.CATEGORY_BROWSABLE)
            }

            val flag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                PackageManager.MATCH_ALL
            } else {
                PackageManager.MATCH_DEFAULT_ONLY
            }
            return packageManager.queryIntentActivities(intent, flag)
                .orEmpty()
                .filter {
                    it.activityInfo.exported && (
                        includeThisApp ||
                            it.activityInfo.packageName != context.packageName
                        )
                }
        }
    }
}
