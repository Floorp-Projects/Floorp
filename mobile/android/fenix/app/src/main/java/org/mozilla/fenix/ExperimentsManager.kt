/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import mozilla.components.service.experiments.Experiments
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings

object ExperimentsManager {
    fun initEtpExperiment(context: Context) {
        // When the `fenix-etp-5651` experiment is active, set up ETP settings and GV policy.
        // Note that this will not take effect the first time the application has launched,
        // since there won't be enough time for the experiments library to get a list of experiments.
        // It will take effect the second time the application is launched.
        Experiments.withExperiment("fenix-etp-5651") { branchName ->
            when (branchName) {
                "control_strict" -> {
                    context.settings().setUseStrictTrackingProtection()
                    context.components.useCases.settingsUseCases.updateTrackingProtection(
                        context.components.core.createTrackingProtectionPolicy()
                    )
                }
                "treatment_standard" -> {
                    context.settings().setUseStandardTrackingProtection()
                    context.components.core.createTrackingProtectionPolicy()
                    context.components.useCases.settingsUseCases.updateTrackingProtection(
                        context.components.core.createTrackingProtectionPolicy()
                    )
                }
                else -> {
                    // No branch matches so we're defaulting to strict
                    context.settings().setUseStrictTrackingProtection()
                    context.components.useCases.settingsUseCases.updateTrackingProtection(
                        context.components.core.createTrackingProtectionPolicy()
                    )
                }
            }
        }
    }
}
