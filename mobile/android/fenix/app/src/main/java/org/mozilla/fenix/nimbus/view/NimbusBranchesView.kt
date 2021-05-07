/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus.view

import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.service.nimbus.ui.NimbusBranchAdapter
import org.mozilla.fenix.nimbus.NimbusBranchesState
import org.mozilla.fenix.nimbus.controller.NimbusBranchesController

/**
 * View used for managing a Nimbus experiment's branches.
 */
class NimbusBranchesView(
    override val containerView: ViewGroup,
    val controller: NimbusBranchesController
) : LayoutContainer {

    private val nimbusAdapter = NimbusBranchAdapter(controller)

    init {
        val recyclerView: RecyclerView = containerView as RecyclerView
        recyclerView.apply {
            adapter = nimbusAdapter
            layoutManager = LinearLayoutManager(containerView.context)
        }
    }

    fun update(state: NimbusBranchesState) {
        nimbusAdapter.updateData(state.branches, state.selectedBranch)
    }
}
