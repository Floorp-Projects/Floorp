/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus.controller

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.service.nimbus.ui.NimbusBranchesAdapterDelegate
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.NimbusBranchesAction
import org.mozilla.fenix.nimbus.NimbusBranchesFragment
import org.mozilla.fenix.nimbus.NimbusBranchesFragmentDirections
import org.mozilla.fenix.nimbus.NimbusBranchesStore

/**
 * [NimbusBranchesFragment] controller. This implements [NimbusBranchesAdapterDelegate] to handle
 * interactions with a Nimbus branch.
 *
 * @param context An Android [Context].
 * @param navController [NavController] used for navigation.
 * @param nimbusBranchesStore An instance of [NimbusBranchesStore] for dispatching
 * [NimbusBranchesAction]s.
 * @param experiments An instance of [NimbusApi] for interacting with the Nimbus experiments.
 * @param experimentId The string experiment-id or "slug" for a Nimbus experiment.
 */
class NimbusBranchesController(
    private val context: Context,
    private val navController: NavController,
    private val nimbusBranchesStore: NimbusBranchesStore,
    private val experiments: NimbusApi,
    private val experimentId: String,
) : NimbusBranchesAdapterDelegate {

    override fun onBranchItemClicked(branch: Branch) {
        val telemetryEnabled = context.settings().isTelemetryEnabled
        val experimentsEnabled = context.settings().isExperimentationEnabled

        updateOptInState(branch)

        if (!telemetryEnabled && !experimentsEnabled) {
            val snackbarText = context.getString(R.string.experiments_snackbar)
            val buttonText = context.getString(R.string.experiments_snackbar_button)
            context.getRootView()?.let { v ->
                FenixSnackbar.make(
                    view = v,
                    FenixSnackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = false,
                )
                    .setText(snackbarText)
                    .setAction(buttonText) {
                        navController.navigateWithBreadcrumb(
                            directions = NimbusBranchesFragmentDirections
                                .actionNimbusBranchesFragmentToDataChoicesFragment(),
                            navigateFrom = "NimbusBranchesController",
                            navigateTo = "ActionNimbusBranchesFragmentToDataChoicesFragment",
                            crashReporter = context.components.analytics.crashReporter,
                        )
                    }
                    .show()
            }
        }
    }

    private fun updateOptInState(branch: Branch) {
        nimbusBranchesStore.dispatch(
            if (experiments.getExperimentBranch(experimentId) != branch.slug) {
                experiments.optInWithBranch(experimentId, branch.slug)
                NimbusBranchesAction.UpdateSelectedBranch(branch.slug)
            } else {
                experiments.optOut(experimentId)
                NimbusBranchesAction.UpdateUnselectBranch
            },
        )
    }
}
