/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.debug

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.runBlocking
import mozilla.components.service.experiments.Configuration
import mozilla.components.service.experiments.Experiment
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.experiments.ExperimentsSnapshot
import mozilla.components.service.experiments.ExperimentsUpdater
import mozilla.components.service.glean.Glean
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class ExperimentsDebugActivityTest {

    private val testPackageName = "mozilla.components.service.experiments.test"
    private val context: Context = ApplicationProvider.getApplicationContext()
    private lateinit var configuration: Configuration

    @Before
    fun setup() {
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        Glean.initialize(context)

        // This makes sure we have a "launch" intent in our package, otherwise
        // it will fail looking for it in `GleanDebugActivityTest`.
        val pm = context.packageManager
        val launchIntent = Intent(Intent.ACTION_MAIN)
        launchIntent.setPackage(testPackageName)
        launchIntent.addCategory(Intent.CATEGORY_LAUNCHER)
        val resolveInfo = ResolveInfo()
        resolveInfo.activityInfo = ActivityInfo()
        resolveInfo.activityInfo.packageName = testPackageName
        resolveInfo.activityInfo.name = "LauncherActivity"
        @Suppress("DEPRECATION")
        Shadows.shadowOf(pm).addResolveInfoForIntent(launchIntent, resolveInfo)
    }

    @Test
    fun `the main activity is correctly started`() {
        // Build the intent that will call our debug activity, with no extra.
        val intent = Intent(context, ExperimentsDebugActivity::class.java)
        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that our main activity was launched.
        assertEquals(testPackageName,
            Shadows.shadowOf(activity.get()).peekNextStartedActivityForResult().intent.`package`!!)
    }

    @Test
    fun `command line extra updateExperiments updates experiments`() {
        // Set the extra values and start the intent.
        val intent = Intent(context, ExperimentsDebugActivity::class.java)
        intent.putExtra(ExperimentsDebugActivity.UPDATE_EXPERIMENTS_EXTRA_KEY, true)
        val activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        val updater: ExperimentsUpdater = spy(ExperimentsUpdater(ApplicationProvider.getApplicationContext<Context>(), Experiments))
        updater.initialize(Configuration())
        Experiments.updater = updater

        activity.create().start().resume()

        // updateJob will not be null if the onCreate method was invoked with the intent extra for
        // updateExperiments
        assertNotNull(activity.get().updateJob)

        // Wait for the update task to join
        runBlocking {
            activity.get().updateJob?.join()
        }

        // Verify that updateExperiments() was called exactly once
        verify(updater, times(1)).updateExperiments()
    }

    @Test
    fun `command line extra setKintoInstance updates source endpoint`() {
        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            ExperimentsDebugActivity::class.java)
        var activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        configuration = Configuration()

        Experiments.initialize(context, configuration)

        activity.create().start().resume()

        // We default to the prod instance, so make sure that we are on the default if no extra is
        // used.
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_PROD, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with the command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "staging")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'staging' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_STAGING, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with a new command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "dev")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'dev' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_DEV, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with a new command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "prod")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'developer' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_PROD, Experiments.updater.source.baseUrl)
    }

    @Test
    fun `command line extra doesn't update source endpoint if invalid instance name used`() {
        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            ExperimentsDebugActivity::class.java)
        var activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        val updater: ExperimentsUpdater = spy(ExperimentsUpdater(ApplicationProvider.getApplicationContext<Context>(), Experiments))
        updater.initialize(Configuration())
        Experiments.updater = updater

        activity.create().start().resume()

        // Right now we default to the prod instance, so make sure that we are on the default if no
        // extra is used
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_PROD, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with the command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "bad-endpoint")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we stayed on the 'production' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_PROD, Experiments.updater.source.baseUrl)
    }

    @Test
    fun `command line extra overrideExperiment updates experiments`() {
        // Fake some experiments to test whether the correct one is active
        val experiment1 = Experiment(
            id = "test-id1",
            branches = listOf(Experiment.Branch(name = "test-branch", ratio = 1)),
            description = "test experiment 1",
            buckets = Experiment.Buckets(start = 0, count = 0),
            match = Experiment.Matcher(
                appId = null,
                appDisplayVersion = null,
                appMinVersion = null,
                appMaxVersion = null,
                debugTags = null,
                deviceManufacturer = null,
                deviceModel = null,
                localeCountry = null,
                localeLanguage = null,
                regions = null
            ),
            lastModified = null,
            schemaModified = null
        )

        // Fake some experiments to test whether the correct one is active
        val experiment2 = Experiment(
            id = "test-id2",
            branches = listOf(Experiment.Branch(name = "test-branch", ratio = 1)),
            description = "test experiment 2",
            buckets = Experiment.Buckets(start = 0, count = 0),
            match = Experiment.Matcher(
                appId = null,
                appDisplayVersion = null,
                appMinVersion = null,
                appMaxVersion = null,
                debugTags = null,
                deviceManufacturer = null,
                deviceModel = null,
                localeCountry = null,
                localeLanguage = null,
                regions = null
            ),
            lastModified = null,
            schemaModified = null
        )

        val experimentsList = listOf(experiment1, experiment2)

        // Set our experiments as the experiment result
        configuration = Configuration()
        Experiments.initialize(context, configuration)
        Experiments.onExperimentsUpdated(ExperimentsSnapshot(experimentsList, null))

        // Set the extra values and start the intent.
        val intent = Intent(context, ExperimentsDebugActivity::class.java)
        intent.putExtra(ExperimentsDebugActivity.OVERRIDE_EXPERIMENT_EXTRA_KEY, "test-id1")
        intent.putExtra(ExperimentsDebugActivity.OVERRIDE_BRANCH_EXTRA_KEY, "test-branch")
        var activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Verify that the experiment exists and is active
        var ex1 = Experiments.getExperiment("test-id1")
        assertNotNull("experiment cannot be null", ex1)
        var ex2 = Experiments.getExperiment("test-id2")
        assertNotNull("experiment cannot be null", ex2)
        assertTrue("experiment must be active",
            Experiments.activeExperiment!!.experiment.id == "test-id1")
        assertFalse("second experiment must not be active",
            Experiments.activeExperiment!!.experiment.id == "test-id2")
        assertTrue("experiment must have correct branch", ex1!!.branches.any {
            it.name == "test-branch"
        })
        activity.pause().stop().destroy()

        // Now clear the experiment using the 'clearAllOverrides' command.
        intent.extras?.clear()
        intent.putExtra(ExperimentsDebugActivity.OVERRIDE_CLEAR_ALL_EXTRA_KEY, true)
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Wait for the clear overrides task to join
        runBlocking {
            activity.get().clearJob?.join()
        }

        // Verify that the experiment exists and is NOT active
        ex1 = Experiments.getExperiment("test-id1")
        assertNotNull("experiment cannot be null", ex1)
        ex2 = Experiments.getExperiment("test-id2")
        assertNotNull("experiment cannot be null", ex2)
        Experiments.activeExperiment?.let {
            assertFalse("first experiment must not be active", it.experiment == ex1!!)
            assertFalse("second experiment must not be active", it.experiment == ex2!!)
        }
    }
}
