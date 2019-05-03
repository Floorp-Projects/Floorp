/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.debug

import android.app.Activity
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.service.experiments.Configuration
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.experiments.ExperimentsUpdater
import mozilla.components.support.base.log.logger.Logger

class ExperimentsDebugActivity : Activity() {
    private val logger = Logger(LOG_TAG)

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal var updateJob: Job? = null
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal var clearJob: Job? = null

    companion object {
        private const val LOG_TAG = "ExperimentsDebugActivity"

        // This is a list of the currently accepted commands
        const val UPDATE_EXPERIMENTS_EXTRA_KEY = "updateExperiments"
        const val SET_KINTO_INSTANCE_EXTRA_KEY = "setKintoInstance"

        // Both of these need to be supplied in order to select both the branch and the
        // ExperimentDescriptor.id of the experiment to override.
        const val OVERRIDE_EXPERIMENT_EXTRA_KEY = "overrideExperiment"
        const val OVERRIDE_BRANCH_EXTRA_KEY = "branch"
        const val OVERRIDE_CLEAR_ALL_EXTRA_KEY = "clearAllOverrides"
    }

    @Suppress("ComplexMethod")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        logger.debug("onCreate")

        if (intent.hasExtra(UPDATE_EXPERIMENTS_EXTRA_KEY)) {
            logger.info("Force update of experiments")
            updateJob = GlobalScope.launch(Dispatchers.IO) {
                Experiments.updater.updateExperiments()
            }
        }

        if (intent.hasExtra(SET_KINTO_INSTANCE_EXTRA_KEY)) {
            val config: Configuration? = when (intent.getStringExtra(SET_KINTO_INSTANCE_EXTRA_KEY)) {
                "dev" -> Experiments.configuration.copy(
                    kintoEndpoint = ExperimentsUpdater.KINTO_ENDPOINT_DEV)
                "staging" -> Experiments.configuration.copy(
                    kintoEndpoint = ExperimentsUpdater.KINTO_ENDPOINT_STAGING)
                "prod" -> Experiments.configuration.copy(
                    kintoEndpoint = ExperimentsUpdater.KINTO_ENDPOINT_PROD)
                else -> null
            }

            config?.let {
                logger.info("Changing Kinto instance to ${config.kintoEndpoint}")
                // Update the default config
                Experiments.configuration = config
                // Reset the source to use the new endpoint
                Experiments.updater.source = Experiments.updater.getExperimentSource(config)
            }
        }

        if (intent.hasExtra(OVERRIDE_CLEAR_ALL_EXTRA_KEY)) {
            logger.info("Clearing all experiment overrides")
            clearJob = GlobalScope.launch(Dispatchers.IO) {
                Experiments.clearAllOverridesNow(applicationContext)
            }
        } else if (intent.hasExtra(OVERRIDE_EXPERIMENT_EXTRA_KEY) &&
            intent.hasExtra(OVERRIDE_BRANCH_EXTRA_KEY)) {
            val experiment = intent.getStringExtra(OVERRIDE_EXPERIMENT_EXTRA_KEY)
            val branch = intent.getStringExtra(OVERRIDE_BRANCH_EXTRA_KEY)
            logger.info("Override to the following experiment/branch:$experiment/$branch")
            Experiments.setOverride(applicationContext, experiment, true, branch)
        }

        val intent = packageManager.getLaunchIntentForPackage(packageName)
        startActivity(intent)

        finish()
    }
}
