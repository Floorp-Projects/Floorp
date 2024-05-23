/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.EXTERNAL
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE
import mozilla.components.feature.app.links.AppLinksUseCases.Companion.ENGINE_SUPPORTED_SCHEMES
import mozilla.components.feature.app.links.RedirectDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.appName

/**
 * This feature implements observer for handling redirects to external apps. The users are asked to
 * confirm their intention before leaving the app if in private session.  These include the Android
 * Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE] `http(s)` URLs.
 *
 * It requires: a [Context], and a [FragmentManager].
 *
 * @param context Context the feature is associated with.
 * @param store Reference to the application's [BrowserStore].
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
class AppLinksFeature(
    private val context: Context,
    private val store: BrowserStore,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    private val dialog: RedirectDialogFragment? = null,
    private val launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(context, launchInApp),
    private val failedToLaunchAction: (fallbackUrl: String?) -> Unit = {},
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase? = null,
    private val engineSupportedSchemes: Set<String> = ENGINE_SUPPORTED_SCHEMES,
    private val shouldPrompt: () -> Boolean = { true },
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    /**
     * Starts observing app links on the selected session.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(sessionId) }
                .distinctUntilChangedBy {
                    it.content.appIntent
                }
                .collect { tab ->
                    tab.content.appIntent?.let {
                        handleAppIntent(tab, it.url, it.appIntent)
                        store.dispatch(ContentAction.ConsumeAppIntentAction(tab.id))
                    }
                }
        }

        findPreviousDialogFragment()?.let {
            fragmentManager?.beginTransaction()?.remove(it)?.commit()
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleAppIntent(tab: SessionState, url: String, appIntent: Intent?) {
        if (appIntent == null) {
            return
        }

        val doNotOpenApp = {
            AppLinksInterceptor.addUserDoNotIntercept(url, appIntent)

            loadUrlIfSchemeSupported(tab, url)
        }

        val doOpenApp = {
            useCases.openAppLink(
                appIntent,
                failedToLaunchAction = failedToLaunchAction,
            )
        }

        @Suppress("ComplexCondition")
        if (isSameCallerAndApp(tab, appIntent) || (!tab.content.private && !shouldPrompt()) ||
            fragmentManager == null
        ) {
            doOpenApp()
            return
        }

        val dialog = getOrCreateDialog(tab.content.private, url)
        dialog.onConfirmRedirect = doOpenApp
        dialog.onCancelRedirect = doNotOpenApp

        if (!isAlreadyADialogCreated()) {
            dialog.showNow(fragmentManager, FRAGMENT_TAG)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getOrCreateDialog(isPrivate: Boolean, url: String): RedirectDialogFragment {
        if (dialog != null) {
            return dialog
        }

        val message = context.getString(
            R.string.mozac_feature_applinks_normal_confirm_dialog_message,
            context.appName,
        )

        return SimpleRedirectDialogFragment.newInstance(
            dialogTitleText = if (isPrivate) {
                R.string.mozac_feature_applinks_confirm_dialog_title
            } else {
                R.string.mozac_feature_applinks_normal_confirm_dialog_title
            },
            dialogMessageString = if (isPrivate) {
                url
            } else {
                message
            },
            positiveButtonText = R.string.mozac_feature_applinks_confirm_dialog_confirm,
            negativeButtonText = R.string.mozac_feature_applinks_confirm_dialog_deny,
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun loadUrlIfSchemeSupported(tab: SessionState, url: String) {
        val schemeSupported = engineSupportedSchemes.contains(Uri.parse(url).scheme)
        if (schemeSupported) {
            loadUrlUseCase?.invoke(
                url = url,
                sessionId = tab.id,
                flags = EngineSession.LoadUrlFlags.select(EXTERNAL, LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE),
            )
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): RedirectDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? RedirectDialogFragment
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun isSameCallerAndApp(tab: SessionState, appIntent: Intent): Boolean {
        return (tab.source as? SessionState.Source.External.CustomTab)?.let { externalSource ->
            when (externalSource.caller?.packageId) {
                null -> false
                appIntent.component?.packageName -> true
                else -> false
            }
        } ?: false
    }
}
