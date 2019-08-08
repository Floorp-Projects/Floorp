/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import mozilla.components.service.fretboard.ExperimentDescriptor

const val EXPERIMENTS_JSON_FILENAME = "experiments.json"
const val EXPERIMENTS_BASE_URL = "https://firefox.settings.services.mozilla.com/v1"
const val EXPERIMENTS_BUCKET_NAME = "main"
// collection name below, see https://bugzilla.mozilla.org/show_bug.cgi?id=1523395 for ownership details
const val EXPERIMENTS_COLLECTION_NAME = "fenix-experiments"

object Experiments {
    val AATestDescriptor = ExperimentDescriptor("AAtest")
    // application services flag to disable the Firefox Sync syncManager
    val asFeatureSyncDisabled = ExperimentDescriptor("asFeatureSyncDisabled")
    // application services flag to disable Firefox Accounts pairing button.
    val asFeatureFxAPairingDisabled = ExperimentDescriptor("asFeatureFxAPairingDisabled")
}

val Context.app: FenixApplication
    get() = applicationContext as FenixApplication

fun Context.isInExperiment(descriptor: ExperimentDescriptor): Boolean =
    app.fretboard.isInExperiment(this, descriptor)
