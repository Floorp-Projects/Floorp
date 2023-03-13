/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.plus
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.R
import org.mozilla.fenix.android.FenixDialogFragment
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentCookieBannerHandlingDetailsDialogBinding
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.trackingprotection.ProtectionsState
import org.mozilla.fenix.trackingprotection.ProtectionsStore

/**
 * A [FenixDialogFragment] that contains all the cookie banner details for a given tab.
 */
class CookieBannerPanelDialogFragment : FenixDialogFragment() {
    @VisibleForTesting
    private lateinit var cookieBannersView: CookieBannerHandlingDetailsView
    private val args by navArgs<CookieBannerPanelDialogFragmentArgs>()
    private var _binding: FragmentCookieBannerHandlingDetailsDialogBinding? = null

    override val gravity: Int get() = args.gravity
    override val layoutId: Int = R.layout.fragment_cookie_banner_handling_details_dialog

    @VisibleForTesting
    internal lateinit var protectionsStore: ProtectionsStore

    // This property is only valid between onCreateView and onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val store = requireComponents.core.store
        val rootView = inflateRootView(container)

        val tab = store.state.findTabOrCustomTab(provideCurrentTabId())
        protectionsStore = StoreProvider.get(this) {
            ProtectionsStore(
                ProtectionsState(
                    tab = tab,
                    url = args.url,
                    isTrackingProtectionEnabled = args.trackingProtectionEnabled,
                    cookieBannerUIMode = args.cookieBannerUIMode,
                    listTrackers = listOf(),
                    mode = ProtectionsState.Mode.Normal,
                    lastAccessedCategory = "",
                ),
            )
        }

        val controller = DefaultCookieBannerDetailsController(
            context = requireContext(),
            ioScope = viewLifecycleOwner.lifecycleScope + Dispatchers.IO,
            cookieBannersStorage = requireComponents.core.cookieBannersStorage,
            protectionsStore = protectionsStore,
            browserStore = requireComponents.core.store,
            fragment = this,
            sessionId = args.sessionId,
            reload = requireComponents.useCases.sessionUseCases.reload,
            navController = { findNavController() },
            sitePermissions = args.sitePermissions,
            gravity = args.gravity,
            getCurrentTab = ::getCurrentTab,
        )

        _binding = FragmentCookieBannerHandlingDetailsDialogBinding.bind(rootView)

        cookieBannersView = CookieBannerHandlingDetailsView(
            context = requireContext(),
            ioScope = viewLifecycleOwner.lifecycleScope + Dispatchers.IO,
            container = binding.cookieBannerDetailsInfoLayout,
            publicSuffixList = requireComponents.publicSuffixList,
            interactor = DefaultCookieBannerDetailsInteractor(controller),
            onDismiss = { dismiss() },
        )

        return rootView
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(protectionsStore) { state ->
            cookieBannersView.update(state)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    @VisibleForTesting
    internal fun provideCurrentTabId(): String = args.sessionId

    private fun getCurrentTab(): SessionState? {
        return requireComponents.core.store.state.findTabOrCustomTab(args.sessionId)
    }
}
