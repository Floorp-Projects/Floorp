/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.nimbus.controller.NimbusBranchesController
import org.mozilla.fenix.nimbus.view.NimbusBranchesView

/**
 * A fragment to show the branches of a Nimbus experiment.
 */
@Suppress("TooGenericExceptionCaught")
class NimbusBranchesFragment : Fragment() {

    private lateinit var nimbusBranchesStore: NimbusBranchesStore
    private lateinit var nimbusBranchesView: NimbusBranchesView
    private lateinit var controller: NimbusBranchesController

    private val args by navArgs<NimbusBranchesFragmentArgs>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view =
            inflater.inflate(R.layout.mozac_service_nimbus_experiment_details, container, false)

        nimbusBranchesStore = StoreProvider.get(this) {
            NimbusBranchesStore(NimbusBranchesState(branches = emptyList()))
        }

        controller = NimbusBranchesController(
            nimbusBranchesStore = nimbusBranchesStore,
            experiments = requireContext().components.analytics.experiments,
            experimentId = args.experimentId
        )

        nimbusBranchesView =
            NimbusBranchesView(view.findViewById(R.id.nimbus_experiment_branches_list), controller)

        loadExperimentBranches()

        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFrom(nimbusBranchesStore) { state ->
            nimbusBranchesView.update(state)
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(args.experimentName)
    }

    private fun loadExperimentBranches() {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val experiments = requireContext().components.analytics.experiments
                val branches = experiments.getExperimentBranches(args.experimentId) ?: emptyList()
                val selectedBranch = experiments.getExperimentBranch(args.experimentId) ?: ""

                nimbusBranchesStore.dispatch(
                    NimbusBranchesAction.UpdateBranches(
                        branches,
                        selectedBranch
                    )
                )
            } catch (e: Throwable) {
                Logger.error("Failed to getActiveExperiments()", e)
            }
        }
    }
}
