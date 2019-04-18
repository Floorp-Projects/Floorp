/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.debug

import android.app.Activity
import android.os.Bundle
import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.service.experiments.Experiments
import mozilla.components.support.base.log.logger.Logger

class ExperimentsDebugActivity : Activity() {
    private val logger = Logger(LOG_TAG)

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal var updateJob: Job? = null

    companion object {
        private const val LOG_TAG = "ExperimentsDebugActivity"

        // This is a list of the currently accepted commands
        const val UPDATE_EXPERIMENTS_EXTRA_KEY = "updateExperiments"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        logger.debug("onCreate")

        if (intent.hasExtra(UPDATE_EXPERIMENTS_EXTRA_KEY)) {
            updateJob = GlobalScope.launch(Dispatchers.IO) {
                Experiments.updater.updateExperiments()
            }
        }

        val intent = packageManager.getLaunchIntentForPackage(packageName)
        startActivity(intent)

        finish()
    }
}
