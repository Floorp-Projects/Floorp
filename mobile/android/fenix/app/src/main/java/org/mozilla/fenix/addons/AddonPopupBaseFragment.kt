/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.ext.requireComponents

/**
 * Provides shared functionality to our fragments for add-on settings and
 * browser/page action popups.
 */
abstract class AddonPopupBaseFragment : Fragment(), EngineSession.Observer, UserInteractionHandler {
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()

    protected var session: SessionState? = null
    protected var engineSession: EngineSession? = null
    private var canGoBack: Boolean = false

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        session?.let {
            promptsFeature.set(
                feature = PromptFeature(
                    fragment = this,
                    store = requireComponents.core.store,
                    customTabId = it.id,
                    fragmentManager = parentFragmentManager,
                    onNeedToRequestPermissions = { permissions ->
                        requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                    },
                    tabsUseCases = requireComponents.useCases.tabsUseCases,
                ),
                owner = this,
                view = view,
            )
        }
    }

    override fun onDestroyView() {
        engineSession?.close()
        session?.let {
            requireComponents.core.store.dispatch(CustomTabListAction.RemoveCustomTabAction(it.id))
        }
        super.onDestroyView()
    }

    override fun onStart() {
        super.onStart()
        engineSession?.register(this)
    }

    override fun onStop() {
        super.onStop()
        engineSession?.unregister(this)
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        session?.let { session ->
            requireComponents.core.store.dispatch(
                ContentAction.UpdatePromptRequestAction(
                    session.id,
                    promptRequest,
                ),
            )
        }
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        if (windowRequest.type == WindowRequest.Type.CLOSE) {
            findNavController().popBackStack()
        } else {
            engineSession?.loadUrl(windowRequest.url)
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { this.canGoBack = canGoBack }
    }

    override fun onBackPressed(): Boolean {
        return if (this.canGoBack) {
            engineSession?.goBack()
            true
        } else {
            false
        }
    }

    protected fun initializeSession(fromEngineSession: EngineSession? = null) {
        engineSession = fromEngineSession ?: requireComponents.core.engine.createSession()
        session = createCustomTab(
            url = "",
            source = SessionState.Source.Internal.CustomTab,
        ).copy(engineState = EngineState(engineSession))
        requireComponents.core.store.dispatch(CustomTabListAction.AddCustomTabAction(session as CustomTabSessionState))
    }

    final override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray,
    ) {
        when (requestCode) {
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.get()?.onPermissionsResult(permissions, grantResults)
        }
    }

    companion object {
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 1
    }
}
