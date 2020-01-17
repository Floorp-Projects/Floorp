/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger
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
 *
 * @param context Context the feature is associated with.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param browserPackageNames Set of browser package names installed.
 * @param unguessableWebUrl URL is not likely to be opened by a native app but will fallback to a browser.
 */
class AppLinksUseCases(
    private val context: Context,
    private val launchInApp: () -> Boolean = { false },
    private val failedToLunch: () -> Unit = { },
    browserPackageNames: Set<String>? = null,
    unguessableWebUrl: String = "https://${UUID.randomUUID()}.net"
) {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val browserPackageNames: Set<String>

    init {
        this.browserPackageNames = browserPackageNames ?: findExcludedPackages(unguessableWebUrl)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun findActivities(intent: Intent): List<ResolveInfo> {
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
    @Suppress("ComplexMethod")
    inner class GetAppLinkRedirect internal constructor(
        private val includeHttpAppLinks: Boolean = false,
        private val ignoreDefaultBrowser: Boolean = false,
        private val includeInstallAppFallback: Boolean = false
    ) {
        operator fun invoke(url: String): AppLinkRedirect {
            val redirectData = createBrowsableIntents(url)
            val isAppIntentHttpOrHttps = redirectData.appIntent?.data?.isHttpOrHttps ?: false

            val appIntent = when {
                redirectData.resolveInfo == null -> null
                includeHttpAppLinks && (ignoreDefaultBrowser ||
                    (redirectData.appIntent != null && isDefaultBrowser(redirectData.appIntent))) -> null
                includeHttpAppLinks -> redirectData.appIntent
                !launchInApp() && isAppIntentHttpOrHttps -> null
                else -> redirectData.appIntent
            }

            val fallbackUrl = when {
                redirectData.fallbackIntent?.data?.isHttpOrHttps == true ->
                    redirectData.fallbackIntent.dataString
                else -> null
            }

            val marketplaceIntent = when {
                launchInApp() -> redirectData.marketplaceIntent
                else -> null
            }

            return AppLinkRedirect(appIntent, fallbackUrl, marketplaceIntent)
        }

        private fun isDefaultBrowser(intent: Intent) =
            findDefaultActivity(intent)?.activityInfo?.packageName == context.packageName

        private fun getNonBrowserActivities(intent: Intent): List<ResolveInfo> {
            return findActivities(intent)
                .map { it.activityInfo.packageName to it }
                .filter { !browserPackageNames.contains(it.first) || intent.`package` == it.first }
                .map { it.second }
        }

        private fun createBrowsableIntents(url: String): RedirectData {
            val intent = Intent.parseUri(url, 0)

            if (intent.action == Intent.ACTION_VIEW) {
                intent.addCategory(Intent.CATEGORY_BROWSABLE)
                intent.component = null
                intent.selector = null
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            }

            val fallbackIntent = intent.getStringExtra(EXTRA_BROWSER_FALLBACK_URL)?.let {
                Intent.parseUri(it, 0)
            }

            val marketplaceIntent = intent.`package`?.let {
                if (includeInstallAppFallback) {
                    Intent.parseUri(MARKET_INTENT_URI_PACKAGE_PREFIX + it, 0)
                } else {
                    null
                }
            }

            if (marketplaceIntent != null) {
                marketplaceIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            }

            val resolveInfoList = intent?.let {
                getNonBrowserActivities(it)
            }
            val resolveInfo = resolveInfoList?.firstOrNull()

            // only target intent for specific app if only one non browser app is found
            if (resolveInfoList?.count() == 1) {
                resolveInfo?.let {
                    intent.`package` = it.activityInfo?.packageName
                }
            }

            val appIntent = when (intent.data) {
                null -> null
                else -> intent
            }

            return RedirectData(appIntent, fallbackIntent, marketplaceIntent, resolveInfo)
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
        operator fun invoke(appIntent: Intent?, failedToLaunchAction: () -> Unit = {}) {
            appIntent?.let {
                try {
                    context.startActivity(it)
                } catch (e: ActivityNotFoundException) {
                    failedToLaunchAction()
                    Logger.error("failed to start third party app activity", e)
                }
            }
        }
    }

    val openAppLink: OpenAppLinkRedirect by lazy { OpenAppLinkRedirect(context) }
    val interceptedAppLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = false,
            includeInstallAppFallback = true
        )
    }
    val appLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = true,
            ignoreDefaultBrowser = false,
            includeInstallAppFallback = false
        )
    }

    private data class RedirectData(
        val appIntent: Intent? = null,
        val fallbackIntent: Intent? = null,
        val marketplaceIntent: Intent? = null,
        val resolveInfo: ResolveInfo? = null
    )
}
