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
import mozilla.components.concept.engine.request.RequestInterceptor
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
 * It provides a [RequestInterceptor] to do the detection and asking of consent.
 *
 * It requires: a [Context], and a [FragmentManager].
 *
 * A [Boolean] flag is provided at construction to allow the feature and use cases to be landed without
 * adjoining UI. The UI will be activated in https://github.com/mozilla-mobile/android-components/issues/2974
 * and https://github.com/mozilla-mobile/android-components/issues/2975.
 *
 * @param alwaysAllowedSchemes List of schemes that will always be allowed to be opened in a third-party
 * app even if [interceptLinkClicks] is `false`.
 * @param alwaysDeniedSchemes List of schemes that will never be opened in a third-party app even if
 * [interceptLinkClicks] is `true`.
 */
class AppLinksFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val interceptLinkClicks: Boolean = false,
    private val alwaysAllowedSchemes: Set<String> = setOf("mailto", "market", "sms", "tel"),
    private val alwaysDeniedSchemes: Set<String> = setOf("javascript"),
    private val fragmentManager: FragmentManager? = null,
    private var dialog: RedirectDialogFragment = SimpleRedirectDialogFragment.newInstance(),
    private val useCases: AppLinksUseCases = AppLinksUseCases(context)
) : LifecycleAwareFeature {

    @VisibleForTesting
    internal val observer: SelectionAwareSessionObserver = object : SelectionAwareSessionObserver(sessionManager) {
        override fun onLoadRequest(
            session: Session,
            url: String,
            triggeredByRedirect: Boolean,
            triggeredByWebContent: Boolean
        ) {
            handleLoadRequest(session, url, triggeredByWebContent)
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

    @VisibleForTesting
    internal fun handleLoadRequest(session: Session, url: String, triggeredByWebContent: Boolean) {
        if (!triggeredByWebContent) {
            return
        }

        // If we're already on the site, and we're clicking around then
        // let's not go to an external app.
        if (url.hostname() == session.url.hostname()) {
            return
        }

        val redirect = useCases.interceptedAppLinkRedirect(url)

        if (redirect.isRedirect()) {
            handleRedirect(redirect, session)
        }
    }

    @SuppressLint("MissingPermission")
    @VisibleForTesting
    internal fun handleRedirect(redirect: AppLinkRedirect, session: Session) {
        if (!redirect.hasExternalApp()) {
            handleFallback(redirect, session)
            return
        }

        redirect.appIntent?.data?.scheme?.let { scheme ->
            if ((!interceptLinkClicks && !alwaysAllowedSchemes.contains(scheme)) ||
                alwaysDeniedSchemes.contains(scheme)) {
                return
            }
        }

        val doOpenApp = {
            useCases.openAppLink(redirect)
        }

        if (!session.private || fragmentManager == null) {
            doOpenApp()
            return
        }

        dialog.setAppLinkRedirect(redirect)
        dialog.onConfirmRedirect = doOpenApp

        if (!isAlreadyADialogCreated()) {
            dialog.show(fragmentManager, FRAGMENT_TAG)
        }
    }

    private fun handleFallback(redirect: AppLinkRedirect, session: Session) {
        redirect.webUrl?.let {
            sessionManager.getOrCreateEngineSession(session).loadUrl(it)
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    @VisibleForTesting
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
