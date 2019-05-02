/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.debug

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.test.core.app.ApplicationProvider
import androidx.work.testing.WorkManagerTestInitHelper
import junit.framework.Assert.assertEquals
import junit.framework.Assert.assertNotNull
import kotlinx.coroutines.runBlocking
import mozilla.components.service.experiments.Configuration
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.experiments.ExperimentsUpdater
import mozilla.components.service.glean.Glean
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows

@RunWith(RobolectricTestRunner::class)
class ExperimentsDebugActivityTest {

    private val testPackageName = "mozilla.components.service.experiments"
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Before
    fun setup() {
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        Glean.initialize(context)
        Experiments.initialize(context)

        // This makes sure we have a "launch" intent in our package, otherwise
        // it will fail looking for it in `GleanDebugActivityTest`.
        val pm = ApplicationProvider.getApplicationContext<Context>().packageManager
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
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            ExperimentsDebugActivity::class.java)
        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that our main activity was launched.
        Assert.assertEquals(testPackageName,
            Shadows.shadowOf(activity.get()).peekNextStartedActivityForResult().intent.`package`!!)
    }

    @Test
    fun `command line extra updateExperiments updates experiments`() {
        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            ExperimentsDebugActivity::class.java)
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

        val updater: ExperimentsUpdater = spy(ExperimentsUpdater(ApplicationProvider.getApplicationContext<Context>(), Experiments))
        updater.initialize(Configuration())
        Experiments.updater = updater

        activity.create().start().resume()

        // Right now we default to the dev instance, so make sure that we are on the default if no
        // extra is used
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_DEV, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with the command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "staging")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'staging' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_STAGING, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with a new command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "prod")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'production' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_PROD, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with a new command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "dev")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we changed to the 'developer' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_DEV, Experiments.updater.source.baseUrl)
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

        // Right now we default to the dev instance, so make sure that we are on the default if no
        // extra is used
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_DEV, Experiments.updater.source.baseUrl)

        // Destroy the activity so we can create a new one with the command added to the intent
        activity.pause().stop().destroy()

        intent.putExtra(ExperimentsDebugActivity.SET_KINTO_INSTANCE_EXTRA_KEY, "bad-endpoint")
        activity = Robolectric.buildActivity(ExperimentsDebugActivity::class.java, intent)

        activity.create().start().resume()

        // Make sure we stayed on the 'developer' instance
        assertEquals(ExperimentsUpdater.KINTO_ENDPOINT_DEV, Experiments.updater.source.baseUrl)
    }
}
