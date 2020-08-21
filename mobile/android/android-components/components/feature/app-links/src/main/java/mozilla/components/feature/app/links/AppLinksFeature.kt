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
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.app.links.RedirectDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.session.SessionUseCases
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
 * @param sessionId The session ID to observe.
 * @param fragmentManager FragmentManager for interacting with fragments.
 * @param dialog The dialog for redirect.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param useCases These use cases allow for the detection of, and opening of links that other apps
 * have registered to open.
 * @param failedToLaunchAction Action to perform when failing to launch in third party app.
 * @param loadUrlUseCase Used to load URL if user decides not to launch in third party app.
 **/
@Suppress("LongParameterList")
class AppLinksFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    private var dialog: RedirectDialogFragment? = null,
    private val launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(context, launchInApp),
    private val failedToLaunchAction: () -> Unit = {},
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase? = null
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

            val doNotOpenApp = {
                loadUrlUseCase?.invoke(url, session, EngineSession.LoadUrlFlags.none())
            }

            if (!session.private || fragmentManager == null) {
                doOpenApp()
                return
            }

            val dialog = getOrCreateDialog()
            dialog.setAppLinkRedirectUrl(url)
            dialog.onConfirmRedirect = doOpenApp
            dialog.onCancelRedirect = doNotOpenApp

            if (!isAlreadyADialogCreated()) {
                dialog.showNow(fragmentManager, FRAGMENT_TAG)
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getOrCreateDialog(): RedirectDialogFragment {
        val existingDialog = dialog
        if (existingDialog != null) {
            return existingDialog
        }

        SimpleRedirectDialogFragment.newInstance().also {
            dialog = it
            return it
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): RedirectDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? RedirectDialogFragment
    }
}
