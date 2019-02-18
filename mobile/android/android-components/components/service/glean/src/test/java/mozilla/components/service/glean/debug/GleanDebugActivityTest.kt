package mozilla.components.service.glean.debug

import android.content.Context
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.robolectric.Robolectric
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import org.robolectric.Shadows.shadowOf

@RunWith(RobolectricTestRunner::class)
class GleanDebugActivityTest {

    private val testPackageName = "mozilla.components.service.glean"

    @Before
    fun setup() {
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
        shadowOf(pm).addResolveInfoForIntent(launchIntent, resolveInfo)
    }

    @Test
    fun `the default configuration is not changed if no extras are provided`() {
        val originalConfig = Configuration()
        Glean.configuration = originalConfig

        // Build the intent that will call our debug activity, with no extra.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        assertNull(intent.extras)

        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Verify that the original configuration and the one after init took place
        // are the same.
        assertEquals(originalConfig, Glean.configuration)
    }

    @Test
    fun `command line extra arguments are correctly parsed`() {
        // Make sure to set a baseline configuration to check against.
        val originalConfig = Configuration()
        Glean.configuration = originalConfig
        assertFalse(originalConfig.logPings)

        // Set the extra values and start the intent.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        intent.putExtra("logPings", true)
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that the configuration option was correctly flipped.
        assertTrue(Glean.configuration.logPings)
    }

    @Test
    fun `the main activity is correctly started`() {
        // Build the intent that will call our debug activity, with no extra.
        val intent = Intent(ApplicationProvider.getApplicationContext<Context>(),
            GleanDebugActivity::class.java)
        // Start the activity through our intent.
        val activity = Robolectric.buildActivity(GleanDebugActivity::class.java, intent)
        activity.create().start().resume()

        // Check that our main activity was launched.
        assertEquals(testPackageName,
            shadowOf(activity.get()).peekNextStartedActivityForResult().intent.`package`!!)
    }
}
