/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import mozilla.components.service.experiments.Experiments
import org.mozilla.fenix.ext.settings

object ExperimentsManager {

    fun optOutSearchWidgetExperiment(context: Context) {
        // Release user has opted out of search widget CFR experiment, reset them to not see it.
        context.settings().setSearchWidgetExperiment(false)
    }

    fun initSearchWidgetExperiment(context: Context) {
        // When the `search-widget-discoverability` experiment is active,set the pref to either
        // show or hide the search widget CFR (given other criteria are met as well).
        // Note that this will not take effect the first time the application has launched,
        // since there won't be enough time for the experiments library to get a list of experiments.
        // It will take effect the second time the application is launched.
        Experiments.withExperiment("fenix-search-widget") { branchName ->
            when (branchName) {
                "control_no_cfr" -> {
                    context.settings().setSearchWidgetExperiment(false)
                }
                "treatment_cfr" -> {
                    context.settings().setSearchWidgetExperiment(true)
                }
                else -> {
                    // No branch matches so we're defaulting to no CFR
                    context.settings().setSearchWidgetExperiment(false)
                }
            }
        }
    }
}
