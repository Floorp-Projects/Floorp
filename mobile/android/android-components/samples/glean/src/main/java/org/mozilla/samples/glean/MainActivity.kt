/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.graphics.Color
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.*
import mozilla.components.service.nimbus.NimbusApi
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.samples.glean.GleanMetrics.BrowserEngagement
import org.mozilla.samples.glean.GleanMetrics.Test
import org.mozilla.samples.glean.library.SamplesGleanLibrary

/**
 * Main Activity of the glean-sample-app
 */
open class MainActivity : AppCompatActivity(), NimbusApi.Observer {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Generate an event when user clicks on the button.
        buttonGenerateData.setOnClickListener {
            // These first two actions, adding to the string list and incrementing the counter are
            // tied to a user lifetime metric which is persistent from launch to launch.

            // Adds the EditText's text content as a new string in the string list metric from the
            // metrics.yaml file.
            Test.stringList.add(etStringListInput.text.toString())
            // Clear current text to help indicate something happened
            etStringListInput.setText("")

            // Increments the test_counter metric from the metrics.yaml file.
            Test.counter.add()

            // This is referencing the event ping named 'click' from the metrics.yaml file. In
            // order to illustrate adding extra information to the event, it is also adding to the
            // 'extras' field a dictionary of values.  Note that the dictionary keys must be
            // declared in the metrics.yaml file under the 'extra_keys' section of an event metric.
            BrowserEngagement.click.record(
                    mapOf(
                            BrowserEngagement.clickKeys.key1 to "extra_value_1",
                            BrowserEngagement.clickKeys.key2 to "extra_value_2"
                    )
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
        // Attach the click listener for the experiments button to the updateExperiments function
        buttonCheckExperiments.setOnClickListener {
            GleanApplication.nimbus.updateExperiments()
        }
        // Register the main activity as a Nimbus observer
        GleanApplication.nimbus.register(this)
    }

    /**
     * Event to indicate that the experiments have been fetched from the endpoint
     */
    override fun onExperimentsFetched() {
        println("Experiments fetched")
    }

    /**
     * Event to indicate that the experiment enrollments have been applied. Developers normally
     * shouldn't care to observe this and rather rely on `onExperimentsFetched` and `withExperiment`
     */
    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
        GleanApplication.nimbus.getExperimentBranch("test-color")?.let { branch ->
            val color = when (branch) {
                "blue" -> Color.BLUE
                "red" -> Color.RED
                "control" -> Color.DKGRAY
                else -> Color.WHITE
            }

            // Dispatch the UI work back to the appropriate thread
            this@MainActivity.runOnUiThread {
                textViewExperimentStatus.setBackgroundColor(color)
                textViewExperimentStatus.text = getString(
                    R.string.experiment_active_branch,
                    "Experiment Branch: $branch")
            }
        }
    }

    /** End Nimbus component functions */
}
