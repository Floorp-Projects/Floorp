/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentTabHistoryDialogBinding
import org.mozilla.fenix.ext.requireComponents

class TabHistoryDialogFragment : BottomSheetDialogFragment() {

    var customTabSessionId: String? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.fragment_tab_history_dialog, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val binding = FragmentTabHistoryDialogBinding.bind(view)

        view.setBackgroundColor(view.context.getColorFromAttr(R.attr.layer1))

        customTabSessionId = requireArguments().getString(EXTRA_SESSION_ID)

        val controller = DefaultTabHistoryController(
            navController = findNavController(),
            goToHistoryIndexUseCase = requireComponents.useCases.sessionUseCases.goToHistoryIndex,
            customTabId = customTabSessionId,
        )
        val tabHistoryView = TabHistoryView(
            container = binding.tabHistoryLayout,
            expandDialog = ::expand,
            interactor = TabHistoryInteractor(controller),
        )

        requireComponents.core.store.flowScoped(viewLifecycleOwner) { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(customTabSessionId)?.content?.history }
                .ifChanged()
                .collect { historyState ->
                    tabHistoryView.updateState(historyState)
                }
        }
    }

    private fun expand() {
        (dialog as BottomSheetDialog).behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    companion object {
        const val EXTRA_SESSION_ID = "activeSessionId"
        val NAME: String = TabHistoryDialogFragment::class.java.canonicalName?.substringAfterLast('.')
            ?: TabHistoryDialogFragment::class.java.simpleName
    }
}
