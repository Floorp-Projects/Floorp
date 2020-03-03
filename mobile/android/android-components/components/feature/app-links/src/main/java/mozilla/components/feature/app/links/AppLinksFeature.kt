/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.app.links.RedirectDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * This feature implements observer for handling redirects to external apps. The users are asked to
 * confirm their intention before leaving the app if in private session.  These include the Android
 * Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE] `http(s)` URLs.
 *
 * It requires: a [Context], and a [FragmentManager].
 *
 * @param context Context the feature is associated with.
 * @param sessionManager Provides access to a centralized registry of all active sessions.
 * @param sessionId the session ID to observe.
 * @param fragmentManager FragmentManager for interacting with fragments.
 * @param dialog The dialog for redirect.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param useCases These use cases allow for the detection of, and opening of links that other apps
 * have registered to open.
 **/
class AppLinksFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    private val dialog: RedirectDialogFragment = SimpleRedirectDialogFragment.newInstance(),
    private val launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(context, launchInApp),
    private val failedToLaunchAction: () -> Unit = {}
) : LifecycleAwareFeature {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val observer: SelectionAwareSessionObserver = object : SelectionAwareSessionObserver(sessionManager) {
        override fun onLaunchIntentRequest(
            session: Session,
            url: String,
            appIntent: Intent?
        ) {
            if (appIntent == null) {
                return
            }

            val doOpenApp = {
                useCases.openAppLink(appIntent, failedToLaunchAction = failedToLaunchAction)
            }

            if (!session.private || fragmentManager == null) {
                doOpenApp()
                return
            }

            dialog.setAppLinkRedirectUrl(url)
            dialog.onConfirmRedirect = doOpenApp

            if (!isAlreadyADialogCreated()) {
                dialog.show(fragmentManager, FRAGMENT_TAG)
            }

            return
        }
    }

    /**
     * Starts observing app links on the selected session.
     */
    override fun start() {
        observer.observeIdOrSelected(sessionId)
        findPreviousDialogFragment()?.let {
            fragmentManager?.beginTransaction()?.remove(it)?.commit()
        }
    }

    override fun stop() {
        observer.stop()
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): RedirectDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? RedirectDialogFragment
    }
}
