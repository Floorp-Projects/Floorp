/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.service.nimbus.ui.NimbusDetailAdapter
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

/**
 * A fragment to show the details of a Nimbus experiment.
 */
class NimbusDetailsFragment : Fragment(R.layout.mozac_service_nimbus_experiment_details) {

    private val args by navArgs<NimbusDetailsFragmentArgs>()
    private var adapter: NimbusDetailAdapter? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bindRecyclerView(view)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(args.experiment)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // Letting go of the resources to avoid memory leak.
        adapter = null
    }

    private fun bindRecyclerView(view: View) {
        val recyclerView = view.findViewById<RecyclerView>(R.id.nimbus_experiment_branches_list)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())

        val shouldRefresh = adapter != null

        // Dummy data until we have the appropriate Nimbus API.
        val branches = listOf(
            "Control",
            "Treatment"
        )

        if (!shouldRefresh) {
            adapter = NimbusDetailAdapter(branches)
        }

        recyclerView.adapter = adapter
    }
}
