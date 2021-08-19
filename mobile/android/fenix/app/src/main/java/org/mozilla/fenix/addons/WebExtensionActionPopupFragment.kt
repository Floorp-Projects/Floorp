/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddOnInternalSettingsBinding
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar

/**
 * A fragment to show the web extension action popup with [EngineView].
 */
class WebExtensionActionPopupFragment : AddonPopupBaseFragment(), EngineSession.Observer {

    private val args by navArgs<WebExtensionActionPopupFragmentArgs>()
    private val coreComponents by lazy { requireComponents.core }
    private val safeArguments get() = requireNotNull(arguments)
    private var sessionConsumed
        get() = safeArguments.getBoolean("isSessionConsumed", false)
        set(value) {
            safeArguments.putBoolean("isSessionConsumed", value)
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Grab the [EngineSession] from the store when the view is created if it is available.
        coreComponents.store.state.extensions[args.webExtensionId]?.popupSession?.let {
            initializeSession(it)
        }

        return inflater.inflate(R.layout.fragment_add_on_internal_settings, container, false)
    }

    override fun onResume() {
        super.onResume()
        val title = args.webExtensionTitle ?: args.webExtensionId
        showToolbar(title)
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val binding = FragmentAddOnInternalSettingsBinding.bind(view)

        val session = engineSession
        // If we have the session, render it otherwise consume it from the store.
        if (session != null) {
            binding.addonSettingsEngineView.render(session)
            consumePopupSession()
        } else {
            consumeFrom(coreComponents.store) { state ->
                state.extensions[args.webExtensionId]?.let { extState ->
                    val popupSession = extState.popupSession
                    if (popupSession != null) {
                        initializeSession(popupSession)
                        binding.addonSettingsEngineView.render(popupSession)
                        popupSession.register(this)
                        consumePopupSession()
                        engineSession = popupSession
                    } else if (sessionConsumed) {
                        // In case we can't retrieve the popup session lets close the fragment,
                        // this can happen when Android recreates the activity.
                        findNavController().popBackStack()
                    }
                }
            }
        }
    }

    private fun consumePopupSession() {
        coreComponents.store.dispatch(
            WebExtensionAction.UpdatePopupSessionAction(args.webExtensionId, popupSession = null)
        )
        sessionConsumed = true
    }
}
