/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.SystemClock
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.feature.app.links.AppLinksUseCases.Companion.ALWAYS_DENY_SCHEMES
import mozilla.components.feature.app.links.AppLinksUseCases.Companion.ENGINE_SUPPORTED_SCHEMES
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl

private const val WWW = "www."
private const val M = "m."
private const val MOBILE = "mobile."
private const val MAPS = "maps."

/**
 * This feature implements use cases for detecting and handling redirects to external apps. The user
 * is asked to confirm her intention before leaving the app. These include the Android Intents,
 * custom schemes and support for [Intent.CATEGORY_BROWSABLE] `http(s)` URLs.
 *
 * In the case of Android Intents that are not installed, and with no fallback, the user is prompted
 * to search the installed market place.
 *
 * It provides use cases to detect and open links openable in third party non-browser apps.
 *
 * It requires: a [Context].
 *
 * A [Boolean] flag is provided at construction to allow the feature and use cases to be landed without
 * adjoining UI. The UI will be activated in https://github.com/mozilla-mobile/android-components/issues/2974
 * and https://github.com/mozilla-mobile/android-components/issues/2975.
 *
 * @param context Context the feature is associated with.
 * @param interceptLinkClicks If {true} then intercept link clicks.
 * @param engineSupportedSchemes List of schemes that the engine supports.
 * @param alwaysDeniedSchemes List of schemes that will never be opened in a third-party app even if
 * [interceptLinkClicks] is `true`.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param useCases These use cases allow for the detection of, and opening of links that other apps
 * have registered to open.
 * @param launchFromInterceptor If {true} then the interceptor will launch the link in third-party apps if available.
 */
class AppLinksInterceptor(
    private val context: Context,
    private val interceptLinkClicks: Boolean = false,
    private val engineSupportedSchemes: Set<String> = ENGINE_SUPPORTED_SCHEMES,
    private val alwaysDeniedSchemes: Set<String> = ALWAYS_DENY_SCHEMES,
    private var launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(
        context,
        launchInApp,
        alwaysDeniedSchemes = alwaysDeniedSchemes,
    ),
    private val launchFromInterceptor: Boolean = false,
) : RequestInterceptor {

    /**
     * Update launchInApp for this instance of AppLinksInterceptor
     * @param launchInApp the new value of launchInApp
     */
    fun updateLaunchInApp(launchInApp: () -> Boolean) {
        this.launchInApp = launchInApp
        useCases.updateLaunchInApp(launchInApp)
    }

    @Suppress("ComplexMethod", "ReturnCount")
    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        val encodedUri = Uri.parse(uri)
        val uriScheme = encodedUri.scheme
        val engineSupportsScheme = engineSupportedSchemes.contains(uriScheme)
        val isAllowedRedirect = (isRedirect && !isSubframeRequest)

        val doNotIntercept = when {
            uriScheme == null -> true
            // A subframe request not triggered by the user should not go to an external app.
            (!hasUserGesture && isSubframeRequest) -> true
            // If request not from an user gesture, allowed redirect and direct navigation
            // or if we're already on the site then let's not go to an external app.
            (
                (!hasUserGesture && !isAllowedRedirect && !isDirectNavigation) ||
                    isSameDomain(lastUri, uri)
                ) && engineSupportsScheme -> true
            // If scheme not in safelist then follow user preference
            (!interceptLinkClicks || !launchInApp()) && engineSupportsScheme -> true
            // Never go to an external app when scheme is in blocklist
            alwaysDeniedSchemes.contains(uriScheme) -> true
            else -> false
        }

        if (doNotIntercept) {
            return null
        }

        val redirect = useCases.interceptedAppLinkRedirect(uri)
        val result = handleRedirect(redirect, uri, engineSupportedSchemes.contains(uriScheme))

        if (redirect.isRedirect()) {
            if (launchFromInterceptor && result is RequestInterceptor.InterceptionResponse.AppIntent) {
                result.appIntent.flags = result.appIntent.flags or Intent.FLAG_ACTIVITY_NEW_TASK
                useCases.openAppLink(result.appIntent)
            }

            return result
        }

        return null
    }

    @SuppressWarnings("ReturnCount")
    @SuppressLint("MissingPermission")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleRedirect(
        redirect: AppLinkRedirect,
        uri: String,
        schemeSupported: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        if (!launchInApp() || inUserDoNotIntercept(uri, redirect.appIntent)) {
            redirect.fallbackUrl?.let {
                return RequestInterceptor.InterceptionResponse.Url(it)
            }
        }

        if (schemeSupported && inUserDoNotIntercept(uri, redirect.appIntent)) {
            return null
        }

        if (!redirect.hasExternalApp()) {
            redirect.marketplaceIntent?.let {
                return RequestInterceptor.InterceptionResponse.AppIntent(it, uri)
            }

            redirect.fallbackUrl?.let {
                return RequestInterceptor.InterceptionResponse.Url(it)
            }

            return null
        }

        redirect.appIntent?.let {
            return RequestInterceptor.InterceptionResponse.AppIntent(it, uri)
        }

        return null
    }

    // Determines if the transition between the two URLs is related.  If the two URLs
    // are from the same website then the app links interceptor will not try to find an application to open it.
    @VisibleForTesting
    internal fun isSameDomain(url1: String?, url2: String?): Boolean {
        return stripCommonSubDomains(url1?.tryGetHostFromUrl()) == stripCommonSubDomains(url2?.tryGetHostFromUrl())
    }

    // Remove subdomains that are ignored when determining if two URLs are from the same website.
    private fun stripCommonSubDomains(url: String?): String? {
        return when {
            url == null -> return null
            url.startsWith(WWW) -> url.replaceFirst(WWW, "")
            url.startsWith(M) -> url.replaceFirst(M, "")
            url.startsWith(MOBILE) -> url.replaceFirst(MOBILE, "")
            url.startsWith(MAPS) -> url.replaceFirst(MAPS, "")
            else -> url
        }
    }

    companion object {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal var userDoNotInterceptCache: MutableMap<Int, Long> = mutableMapOf()

        @VisibleForTesting
        internal fun getCacheKey(url: String, appIntent: Intent?): Int? {
            return Uri.parse(url)?.let { uri ->
                when {
                    appIntent?.component?.packageName != null -> appIntent.component?.packageName
                    !uri.isHttpOrHttps -> uri.scheme
                    else -> uri.host // worst case we do not prompt again on this host
                }.hashCode()
            }
        }

        @VisibleForTesting
        internal fun inUserDoNotIntercept(url: String, appIntent: Intent?): Boolean {
            val cacheKey = getCacheKey(url, appIntent)
            val cacheTimeStamp = userDoNotInterceptCache[cacheKey]
            val currentTimeStamp = SystemClock.elapsedRealtime()

            return cacheTimeStamp != null &&
                currentTimeStamp <= (cacheTimeStamp + APP_LINKS_DO_NOT_OPEN_CACHE_INTERVAL)
        }

        internal fun addUserDoNotIntercept(url: String, appIntent: Intent?) {
            val cacheKey = getCacheKey(url, appIntent)
            cacheKey?.let {
                userDoNotInterceptCache[it] = SystemClock.elapsedRealtime()
            }
        }

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val APP_LINKS_DO_NOT_OPEN_CACHE_INTERVAL = 60 * 60 * 1000L // 1 hour
    }
}
