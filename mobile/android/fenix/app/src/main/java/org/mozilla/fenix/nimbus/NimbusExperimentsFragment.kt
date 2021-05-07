/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import mozilla.components.service.nimbus.ui.NimbusExperimentAdapter
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.nimbus.view.NimbusExperimentsView

/**
 * Fragment use for managing Nimbus experiments.
 */
@Suppress("TooGenericExceptionCaught")
class NimbusExperimentsFragment : Fragment(R.layout.mozac_service_nimbus_experiments) {

    private var adapter: NimbusExperimentAdapter? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bindRecyclerView(view)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_nimbus_experiments))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // Letting go of the resources to avoid memory leak.
        adapter = null
    }

    private fun bindRecyclerView(view: View) {
        val experimentsView = NimbusExperimentsView(
            navController = findNavController()
        )

        val recyclerView = view.findViewById<RecyclerView>(R.id.nimbus_experiments_list)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())

        val shouldRefresh = adapter != null

        lifecycleScope.launch(IO) {
            try {
                val experiments =
                    requireContext().components.analytics.experiments.getAvailableExperiments()

                lifecycleScope.launch(Main) {
                    runIfFragmentIsAttached {
                        if (!shouldRefresh) {
                            adapter = NimbusExperimentAdapter(
                                experimentsView,
                                experiments
                            )
                        }

                        view.findViewById<TextView>(R.id.nimbus_experiments_empty_message).isVisible =
                            experiments.isEmpty()
                        recyclerView.adapter = adapter
                    }
                }
            } catch (e: Throwable) {
                Logger.error("Failed to getActiveExperiments()", e)
                view.findViewById<TextView>(R.id.nimbus_experiments_empty_message).isVisible = true
            }
        }
    }
}
