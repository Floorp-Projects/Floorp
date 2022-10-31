/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.graphics.Color
import android.os.Bundle
import androidx.annotation.MainThread
import androidx.appcompat.app.AppCompatActivity
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.samples.glean.GleanMetrics.BrowserEngagement
import org.mozilla.samples.glean.GleanMetrics.Test
import org.mozilla.samples.glean.databinding.ActivityMainBinding
import org.mozilla.samples.glean.library.SamplesGleanLibrary

/**
 * Main Activity of the glean-sample-app
 */
open class MainActivity : AppCompatActivity(), NimbusInterface.Observer {
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)

        setContentView(binding.root)

        // Generate an event when user clicks on the button.
        binding.buttonGenerateData.setOnClickListener {
            // These first two actions, adding to the string list and incrementing the counter are
            // tied to a user lifetime metric which is persistent from launch to launch.

            // Adds the EditText's text content as a new string in the string list metric from the
            // metrics.yaml file.
            Test.stringList.add(binding.etStringListInput.text.toString())
            // Clear current text to help indicate something happened
            binding.etStringListInput.setText("")

            // Increments the test_counter metric from the metrics.yaml file.
            Test.counter.add()

            // This is referencing the event ping named 'click' from the metrics.yaml file. In
            // order to illustrate adding extra information to the event, it is also adding to the
            // 'extras' field a dictionary of values.  Note that the dictionary keys must be
            // declared in the metrics.yaml file under the 'extra_keys' section of an event metric.
            BrowserEngagement.click.record(
                BrowserEngagement.ClickExtra(
                    key1 = "extra_value_1",
                    key2 = "extra_value_2",
                ),
            )
        }

        Test.timespan.stop()

        // Update some metrics from a third-party library
        SamplesGleanLibrary.recordMetric()
        SamplesGleanLibrary.recordExperiment()

        // The following is not relevant to the Glean SDK, but to the Nimbus experiments library
        setupNimbusExperiments()
    }

    /** Begin Nimbus component specific functions */

    /**
     * This sets up the update receiver and sets the onClickListener for the "Update Experiments"
     * button. This is not relevant to the Glean SDK, but to the Nimbus experiments library.
     */
    private fun setupNimbusExperiments() {
        // Register the main activity as a Nimbus observer
        GleanApplication.nimbus.register(this)

        // Attach the click listener for the experiments button to the updateExperiments function
        binding.buttonCheckExperiments.setOnClickListener {
            // Once the experiments are fetched, then the activity's (a Nimbus observer)
            // `onExperimentFetched()` method is called.
            GleanApplication.nimbus.fetchExperiments()
        }

        configureButton()
    }

    /**
     * Event to indicate that the experiments have been fetched from the endpoint
     */
    override fun onExperimentsFetched() {
        println("Experiments fetched")
        GleanApplication.nimbus.applyPendingExperiments()
    }

    /**
     * Event to indicate that the experiment enrollments have been applied. Developers normally
     * shouldn't care to observe this and rather rely on `onExperimentsFetched` and `withExperiment`
     */
    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
        runOnUiThread {
            configureButton()
        }
    }

    @MainThread
    private fun configureButton() {
        val nimbus = GleanApplication.nimbus
        val branch = nimbus.getExperimentBranch("sample-experiment-feature")

        val color = when (branch) {
            "blue" -> Color.BLUE
            "red" -> Color.RED
            "control" -> Color.DKGRAY
            else -> Color.WHITE
        }
        val text = when (branch) {
            null -> getString(R.string.experiment_not_active)
            else -> getString(R.string.experiment_active_branch, branch)
        }

        binding.textViewExperimentStatus.setBackgroundColor(color)
        binding.textViewExperimentStatus.text = text
    }

    /** End Nimbus component functions */
}
