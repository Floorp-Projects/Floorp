/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import org.mozilla.fenix.R
import org.mozilla.fenix.android.FenixDialogFragment
import org.mozilla.fenix.databinding.FragmentConnectionDetailsDialogBinding
import org.mozilla.fenix.ext.requireComponents

@ExperimentalCoroutinesApi
class ConnectionPanelDialogFragment : FenixDialogFragment() {
    @VisibleForTesting
    private lateinit var connectionView: ConnectionDetailsView
    private val args by navArgs<ConnectionPanelDialogFragmentArgs>()
    private var _binding: FragmentConnectionDetailsDialogBinding? = null

    override val gravity: Int get() = args.gravity
    override val layoutId: Int = R.layout.fragment_connection_details_dialog
    // This property is only valid between onCreateView and onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val rootView = inflateRootView(container)

        val controller = DefaultConnectionDetailsController(
            context = requireContext(),
            fragment = this,
            navController = { findNavController() },
            sitePermissions = args.sitePermissions,
            gravity = args.gravity,
            getCurrentTab = ::getCurrentTab
        )

        val interactor = ConnectionDetailsInteractor(controller)
        _binding = FragmentConnectionDetailsDialogBinding.bind(rootView)

        connectionView = ConnectionDetailsView(
            binding.connectionDetailsInfoLayout,
            icons = requireComponents.core.icons,
            interactor = interactor
        )

        return rootView
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        connectionView.update(
            WebsiteInfoState.createWebsiteInfoState(
                args.url,
                args.title,
                args.isSecured,
                args.certificateName
            )
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }

    private fun getCurrentTab(): SessionState? {
        return requireComponents.core.store.state.findTabOrCustomTab(args.sessionId)
    }
}
