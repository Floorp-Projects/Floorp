/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import androidx.annotation.VisibleForTesting
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import java.util.UUID

private const val EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url"
private const val MARKET_INTENT_URI_PACKAGE_PREFIX = "market://details?id="

/**
 * These use cases allow for the detection of, and opening of links that other apps have registered
 * an [IntentFilter]s to open.
 *
 * Care is taken to:
 *  * resolve [intent://] links, including [S.browser_fallback_url]
 *  * provide a fallback to the installed marketplace app (e.g. on Google Android, the Play Store).
 *  * open HTTP(S) links with an external app.
 *
 * Since browsers are able to open HTTPS pages, existing browser apps are excluded from the list of
 * apps that trigger a redirect to an external app.
 */
class AppLinksUseCases(
    private val context: Context,
    browserPackageNames: Set<String>? = null,
    unguessableWebUrl: String = "https://${UUID.randomUUID()}.net"
) {
    @VisibleForTesting
    val browserPackageNames: Set<String>

    init {
        this.browserPackageNames = browserPackageNames ?: findExcludedPackages(unguessableWebUrl)
    }

    private fun findActivities(intent: Intent): List<ResolveInfo> {
        return context.packageManager
            .queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY) ?: emptyList()
    }

    private fun findDefaultActivity(intent: Intent): ResolveInfo? {
        return context.packageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY)
    }

    private fun findExcludedPackages(randomWebURLString: String): Set<String> {
        // We generate a URL is not likely to be opened by a native app
        // but will fallback to a browser.
        // In this way, we're looking for only the browsers — including us.
        return findActivities(Intent.parseUri(randomWebURLString, 0).addCategory(Intent.CATEGORY_BROWSABLE))
            .map { it.activityInfo.packageName }
            .toHashSet()
    }

    /**
     * Parse a URL and check if it can be handled by an app elsewhere on the Android device.
     * If that app is not available, then a market place intent is also provided.
     *
     * It will also provide a fallback.
     *
     * @param includeHttpAppLinks If {true} then test URLs that start with {http} and {https}.
     * @param ignoreDefaultBrowser If {true} then do not offer an app link if the user has
     * selected this browser as default previously. This is only applicable if {includeHttpAppLinks}
     * is true.
     * @param includeInstallAppFallback If {true} then offer an app-link to the installed market app
     * if no web fallback is available.
     */
    inner class GetAppLinkRedirect internal constructor(
        private val includeHttpAppLinks: Boolean = false,
        private val ignoreDefaultBrowser: Boolean = false,
        private val includeInstallAppFallback: Boolean = false
    ) {
        operator fun invoke(url: String): AppLinkRedirect {
            val intents = createBrowsableIntents(url)
            val appIntent = if (includeHttpAppLinks) {
                intents.firstOrNull { getNonBrowserActivities(it).isNotEmpty() }
                    ?.let {
                        // The user may have decided to keep opening this type of link in this browser.
                        if (ignoreDefaultBrowser &&
                            findDefaultActivity(it)?.activityInfo?.packageName == context.packageName) {
                            // in which case, this isn't an app intent anymore.
                            null
                        } else {
                            it
                        }
                    }
            } else {
                intents.filter { it.data?.isHttpOrHttps != true }
                    .firstOrNull {
                        getNonBrowserActivities(it).isNotEmpty()
                    }
            }

            val webUrls = intents.mapNotNull {
                if (it.data?.isHttpOrHttps == true) it.dataString else null
            }

            val webUrl = webUrls.firstOrNull { it != url } ?: webUrls.firstOrNull()

            return AppLinkRedirect(appIntent, webUrl, webUrl != url)
        }

        private fun getNonBrowserActivities(intent: Intent): List<ResolveInfo> {
            return findActivities(intent)
                .map { it.activityInfo.packageName to it }
                .filter { !browserPackageNames.contains(it.first) }
                .map { it.second }
        }

        private fun createBrowsableIntents(url: String): List<Intent> {
            val intent = Intent.parseUri(url, 0)

            if (intent.action == Intent.ACTION_VIEW) {
                intent.addCategory(Intent.CATEGORY_BROWSABLE)
            }

            return when (intent.data?.isHttpOrHttps) {
                null -> emptyList()
                true -> listOf(intent)
                false -> {
                    // Non http[s] schemes:

                    val fallback = intent.getStringExtra(EXTRA_BROWSER_FALLBACK_URL)?.let {
                        Intent.parseUri(it, 0)
                    }

                    val marketplaceIntent = intent.`package`?.let {
                        if (includeInstallAppFallback) {
                            Intent.parseUri(MARKET_INTENT_URI_PACKAGE_PREFIX + it, 0)
                        } else {
                            null
                        }
                    }

                    return listOfNotNull(intent, fallback, marketplaceIntent)
                }
            }
        }
    }

    /**
     * Open an external app with the redirect created by the [GetAppLinkRedirect].
     *
     * This does not do any additional UI other than the chooser that Android may provide the user.
     */
    class OpenAppLinkRedirect internal constructor(
        private val context: Context
    ) {
        operator fun invoke(redirect: AppLinkRedirect) {
            val intent = redirect.appIntent ?: return
            context.startActivity(intent)
        }
    }

    val openAppLink: OpenAppLinkRedirect by lazy { OpenAppLinkRedirect(context) }
    val interceptedAppLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = false,
            includeInstallAppFallback = false
        )
    }
    val appLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = true,
            ignoreDefaultBrowser = false,
            includeInstallAppFallback = false
        )
    }
}
