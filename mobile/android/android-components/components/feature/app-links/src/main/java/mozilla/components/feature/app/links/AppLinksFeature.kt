/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.webkit.URLUtil
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.app.links.RedirectDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes

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
 * It requires: a [Context], and a [FragmentManager].
 *
 * A [Boolean] flag is provided at construction to allow the feature and use cases to be landed without
 * adjoining UI. The UI will be activated in https://github.com/mozilla-mobile/android-components/issues/2974
 * and https://github.com/mozilla-mobile/android-components/issues/2975.
 *
 * @param context Context the feature is associated with.
 * @param sessionManager Provides access to a centralized registry of all active sessions.
 * @param sessionId the session ID to observe.
 * @param interceptLinkClicks If {true} then intercept link clicks.
 * @param alwaysAllowedSchemes List of schemes that will always be allowed to be opened in a third-party
 * app even if [interceptLinkClicks] is `false`.
 * @param alwaysDeniedSchemes List of schemes that will never be opened in a third-party app even if
 * [interceptLinkClicks] is `true`.
 * @param fragmentManager FragmentManager for interacting with fragments.
 * @param dialog The dialog for redirect.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param useCases These use cases allow for the detection of, and opening of links that other apps
 * have registered to open.
 */
class AppLinksFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val interceptLinkClicks: Boolean = false,
    private val alwaysAllowedSchemes: Set<String> = setOf("mailto", "market", "sms", "tel"),
    private val alwaysDeniedSchemes: Set<String> = setOf("javascript", "about"),
    private val fragmentManager: FragmentManager? = null,
    private var dialog: RedirectDialogFragment = SimpleRedirectDialogFragment.newInstance(),
    private val launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(context, launchInApp)
) : LifecycleAwareFeature {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val observer: SelectionAwareSessionObserver = object : SelectionAwareSessionObserver(sessionManager) {
        override fun onLoadRequest(
            session: Session,
            url: String,
            triggeredByRedirect: Boolean,
            triggeredByWebContent: Boolean
        ): Boolean {
            return handleLoadRequest(session, url, triggeredByWebContent)
        }
    }

    /**
     * Starts observing app links on the selected session.
     */
    override fun start() {
        if (interceptLinkClicks || alwaysAllowedSchemes.isNotEmpty()) {
            observer.observeIdOrSelected(sessionId)
        }
        findPreviousDialogFragment()?.let {
            reAttachOnConfirmRedirectListener(it)
        }
    }

    override fun stop() {
        if (interceptLinkClicks || alwaysAllowedSchemes.isNotEmpty()) {
            observer.stop()
        }
    }

    @SuppressWarnings("ReturnCount")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleLoadRequest(
        session: Session,
        url: String,
        triggeredByWebContent: Boolean
    ): Boolean {
        if (!triggeredByWebContent) {
            return false
        }

        // If we're already on the site, and we're clicking around then
        // let's not go to an external app.
        if (url.hostname() == session.url.hostname()) {
            return false
        }

        val redirect = useCases.interceptedAppLinkRedirect(url)

        if (redirect.isRedirect()) {
            return handleRedirect(redirect, session)
        }

        return false
    }

    @SuppressWarnings("ReturnCount")
    @SuppressLint("MissingPermission")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleRedirect(redirect: AppLinkRedirect, session: Session): Boolean {
        if (!redirect.hasExternalApp()) {
            return handleFallback(redirect, session)
        }

        redirect.appIntent?.data?.scheme?.let { scheme ->
            if ((!interceptLinkClicks && !alwaysAllowedSchemes.contains(scheme)) ||
                alwaysDeniedSchemes.contains(scheme)) {
                return false
            }
        }

        val doOpenApp = {
            useCases.openAppLink(redirect)
        }

        if (!session.private || fragmentManager == null) {
            doOpenApp()
            return true
        }

        dialog.setAppLinkRedirect(redirect)
        dialog.onConfirmRedirect = doOpenApp

        if (!isAlreadyADialogCreated()) {
            dialog.show(fragmentManager, FRAGMENT_TAG)
        }

        return true
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleFallback(redirect: AppLinkRedirect, session: Session): Boolean {
        redirect.fallbackUrl?.let {
            val engineSession = sessionManager.getOrCreateEngineSession(session)
            engineSession.loadUrl(it)
            return true
        }

        return false
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun reAttachOnConfirmRedirectListener(previousDialog: RedirectDialogFragment?) {
        previousDialog?.apply {
            this@AppLinksFeature.dialog = this
        }
    }

    private fun findPreviousDialogFragment(): RedirectDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? RedirectDialogFragment
    }

    private fun String.hostname() =
        if (URLUtil.isValidUrl(this)) {
            Uri.parse(this).hostWithoutCommonPrefixes
        } else {
            null
        }
}
