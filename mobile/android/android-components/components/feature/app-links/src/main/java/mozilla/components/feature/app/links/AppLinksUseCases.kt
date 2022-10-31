/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.net.Uri
import android.os.SystemClock
import android.provider.Browser.EXTRA_APPLICATION_ID
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.pm.isPackageInstalled
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import java.lang.Exception
import java.lang.NullPointerException
import java.lang.NumberFormatException
import java.net.URISyntaxException

private const val EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url"
private const val MARKET_INTENT_URI_PACKAGE_PREFIX = "market://details?id="

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val APP_LINKS_CACHE_INTERVAL = 30 * 1000L // 30 seconds

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
 * @param alwaysDeniedSchemes List of schemes that will never be opened in a third-party app.
 */
class AppLinksUseCases(
    private val context: Context,
    private val launchInApp: () -> Boolean = { false },
    private val alwaysDeniedSchemes: Set<String> = ALWAYS_DENY_SCHEMES,
) {
    @Suppress(
        "QueryPermissionsNeeded", // We expect our browsers to have the QUERY_ALL_PACKAGES permission
        "TooGenericExceptionCaught",
    )
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun findActivities(intent: Intent): List<ResolveInfo> {
        return try {
            context.packageManager
                .queryIntentActivities(intent, PackageManager.GET_RESOLVED_FILTER)
        } catch (e: RuntimeException) {
            Logger("AppLinksUseCases").error("failed to query activities", e)
            emptyList()
        }
    }

    private fun findDefaultActivity(intent: Intent): ResolveInfo? {
        return context.packageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY)
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
        private val includeInstallAppFallback: Boolean = false,
    ) {
        operator fun invoke(url: String): AppLinkRedirect {
            val urlHash = (url + includeHttpAppLinks + ignoreDefaultBrowser + includeHttpAppLinks).hashCode()
            val currentTimeStamp = SystemClock.elapsedRealtime()
            // since redirectCache is mutable, get the latest
            val cache = redirectCache
            if (cache != null && urlHash == cache.cachedUrlHash &&
                currentTimeStamp <= cache.cacheTimeStamp + APP_LINKS_CACHE_INTERVAL
            ) {
                return cache.cachedAppLinkRedirect
            }

            val redirectData = createBrowsableIntents(url)
            val isAppIntentHttpOrHttps = redirectData.appIntent?.data?.isHttpOrHttps ?: false
            val isEngineSupportedScheme = ENGINE_SUPPORTED_SCHEMES.contains(Uri.parse(url).scheme)

            val appIntent = when {
                redirectData.resolveInfo == null && isEngineSupportedScheme -> null
                redirectData.resolveInfo == null && redirectData.marketplaceIntent != null -> null
                includeHttpAppLinks && (
                    ignoreDefaultBrowser ||
                        (redirectData.appIntent != null && isDefaultBrowser(redirectData.appIntent))
                    ) -> null
                includeHttpAppLinks && isAppIntentHttpOrHttps -> redirectData.appIntent
                !launchInApp() && ENGINE_SUPPORTED_SCHEMES.contains(Uri.parse(url).scheme) -> null
                else -> redirectData.appIntent
            }

            val fallbackUrl = when {
                redirectData.fallbackIntent?.data?.isHttpOrHttps == true ->
                    redirectData.fallbackIntent.dataString
                else -> null
            }

            // no need to check marketplace intent since it is only set if a package is set in the intent
            val appLinkRedirect = AppLinkRedirect(appIntent, fallbackUrl, redirectData.marketplaceIntent)
            redirectCache = AppLinkRedirectCache(currentTimeStamp, urlHash, appLinkRedirect)
            return appLinkRedirect
        }

        private fun isDefaultBrowser(intent: Intent) =
            findDefaultActivity(intent)?.activityInfo?.packageName == context.packageName

        private fun createBrowsableIntents(url: String): RedirectData {
            val intent = safeParseUri(url, Intent.URI_INTENT_SCHEME)
            val fallbackIntent = intent?.getStringExtra(EXTRA_BROWSER_FALLBACK_URL)?.let {
                safeParseUri(it, 0)
            }

            val marketplaceIntent = intent?.`package`?.let {
                if (includeInstallAppFallback &&
                    !context.packageManager.isPackageInstalled(it)
                ) {
                    safeParseUri(MARKET_INTENT_URI_PACKAGE_PREFIX + it, 0)
                } else {
                    null
                }
            }

            if (marketplaceIntent != null) {
                marketplaceIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            }

            val appIntent = when {
                intent?.data == null -> null
                alwaysDeniedSchemes.contains(intent.data?.scheme) -> null
                else -> intent
            }

            appIntent?.let {
                it.addCategory(Intent.CATEGORY_BROWSABLE)
                it.component = null
                it.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                it.selector?.addCategory(Intent.CATEGORY_BROWSABLE)
                it.selector?.component = null
                it.putExtra(EXTRA_APPLICATION_ID, context.packageName)
            }

            val resolveInfoList = appIntent?.let {
                findActivities(appIntent).filter {
                    it.filter != null &&
                        !(it.filter.countDataPaths() == 0 && it.filter.countDataAuthorities() == 0)
                }
            }
            val resolveInfo = resolveInfoList?.firstOrNull()

            // only target intent for specific app if only one non browser app is found
            if (resolveInfoList?.count() == 1) {
                resolveInfo?.let {
                    appIntent.component = ComponentName(it.activityInfo.packageName, it.activityInfo.name)
                }
            }

            return RedirectData(appIntent, fallbackIntent, marketplaceIntent, resolveInfo)
        }
    }

    /**
     * Open an external app with the redirect created by the [GetAppLinkRedirect].
     *
     * This does not do any additional UI other than the chooser that Android may provide the user.
     */
    @Suppress("TooGenericExceptionCaught")
    inner class OpenAppLinkRedirect internal constructor(
        private val context: Context,
    ) {
        /**
         * Tries to open an external app for the provided [appIntent]. Invokes [failedToLaunchAction]
         * in case an exception is thrown opening the app.
         *
         * @param appIntent the [Intent] to open the external app for.
         * @param launchInNewTask whether or not the app should be launched in a new task.
         * @param failedToLaunchAction callback invoked in case opening the external app fails.
         */
        operator fun invoke(
            appIntent: Intent?,
            launchInNewTask: Boolean = true,
            failedToLaunchAction: () -> Unit = {},
        ) {
            appIntent?.let {
                try {
                    val scheme = appIntent.data?.scheme
                    if (scheme != null && alwaysDeniedSchemes.contains(scheme)) {
                        return
                    }

                    if (launchInNewTask) {
                        it.flags = it.flags or Intent.FLAG_ACTIVITY_NEW_TASK
                    }
                    context.startActivity(it)
                } catch (e: Exception) {
                    when (e) {
                        is ActivityNotFoundException, is SecurityException, is NullPointerException -> {
                            failedToLaunchAction()
                            Logger.error("failed to start third party app activity", e)
                        }
                        else -> throw e
                    }
                }
            }
        }
    }

    @VisibleForTesting
    internal fun safeParseUri(uri: String, flags: Int): Intent? {
        return try {
            val intent = Intent.parseUri(uri, flags)
            if (context.packageName != null && context.packageName == intent?.`package`) {
                // Ignore intents that would open in the browser itself
                null
            } else {
                intent
            }
        } catch (e: URISyntaxException) {
            Logger.error("failed to parse URI", e)
            null
        } catch (e: NumberFormatException) {
            Logger.error("failed to parse URI", e)
            null
        }
    }

    val openAppLink: OpenAppLinkRedirect by lazy { OpenAppLinkRedirect(context) }
    val interceptedAppLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = false,
            includeInstallAppFallback = true,
        )
    }
    val appLinkRedirect: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = true,
            ignoreDefaultBrowser = false,
            includeInstallAppFallback = false,
        )
    }
    val appLinkRedirectIncludeInstall: GetAppLinkRedirect by lazy {
        GetAppLinkRedirect(
            includeHttpAppLinks = true,
            ignoreDefaultBrowser = false,
            includeInstallAppFallback = true,
        )
    }
    private data class RedirectData(
        val appIntent: Intent? = null,
        val fallbackIntent: Intent? = null,
        val marketplaceIntent: Intent? = null,
        val resolveInfo: ResolveInfo? = null,
    )

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal data class AppLinkRedirectCache(
        var cacheTimeStamp: Long,
        var cachedUrlHash: Int,
        var cachedAppLinkRedirect: AppLinkRedirect,
    )

    companion object {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal var redirectCache: AppLinkRedirectCache? = null

        // list of scheme from https://searchfox.org/mozilla-central/source/netwerk/build/components.conf
        internal val ENGINE_SUPPORTED_SCHEMES: Set<String> = setOf(
            "about", "data", "file", "ftp", "http",
            "https", "moz-extension", "moz-safe-about", "resource", "view-source", "ws", "wss", "blob",
        )

        internal val ALWAYS_DENY_SCHEMES: Set<String> = setOf("jar", "file", "javascript", "data", "about")
    }
}
