/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.os.Build
import mozilla.components.lib.dataprotect.SecurePrefsReliabilityExperiment
import mozilla.components.service.nimbus.NimbusApi
import org.mozilla.fenix.experiments.ExperimentBranch
import org.mozilla.fenix.experiments.Experiments
import org.mozilla.fenix.ext.withExperiment

/**
 * Allows starting a quick test of ACs SecureAbove22Preferences that will emit Facts
 * for the basic operations and allow us to log them for later evaluation of APIs stability.
 */
class SecurePrefsTelemetry(
    private val appContext: Context,
    private val experiments: NimbusApi
) {
    suspend fun startTests() {
        // The Android Keystore is used to secure the shared prefs only on API 23+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // These tests should run only if the experiment is live
            experiments.withExperiment(Experiments.ANDROID_KEYSTORE) { experimentBranch ->
                // .. and this device is not in the control group.
                if (experimentBranch == ExperimentBranch.TREATMENT) {
                        SecurePrefsReliabilityExperiment(appContext)()
                }
            }
        }
    }
}
