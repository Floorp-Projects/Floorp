/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.widget

import android.app.Activity
import android.app.Activity.RESULT_OK
import android.content.ComponentName
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.speech.RecognizerIntent.ACTION_RECOGNIZE_SPEECH
import android.speech.RecognizerIntent.EXTRA_RESULTS
import androidx.activity.result.ActivityResult
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity.Companion.PREVIOUS_INTENT
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity.Companion.SPEECH_PROCESSING
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.android.controller.ActivityController
import org.robolectric.shadows.ShadowActivity

@RunWith(RobolectricTestRunner::class)
class BaseVoiceSearchActivityTest {

    private lateinit var controller: ActivityController<BaseVoiceSearchActivityExtendedForTests>
    private lateinit var activity: BaseVoiceSearchActivityExtendedForTests
    private lateinit var shadow: ShadowActivity

    @Before
    fun setup() {
        val intent = Intent()
        intent.putExtra(SPEECH_PROCESSING, true)

        controller = Robolectric.buildActivity(BaseVoiceSearchActivityExtendedForTests::class.java, intent)
        activity = controller.get()
        shadow = shadowOf(activity)
    }

    private fun allowVoiceIntentToResolveActivity() {
        val shadowPackageManager = shadowOf(testContext.packageManager)
        val component = ComponentName("com.test", "Test")
        shadowPackageManager.addActivityIfNotPresent(component)
        shadowPackageManager.addIntentFilterForActivity(
            component,
            IntentFilter(ACTION_RECOGNIZE_SPEECH).apply { addCategory(Intent.CATEGORY_DEFAULT) },
        )
    }

    @Test
    fun `process intent with speech processing set to true`() {
        val intent = Intent()
        intent.putStringArrayListExtra(EXTRA_RESULTS, ArrayList<String>(listOf("hello world")))
        val activityResult = ActivityResult(RESULT_OK, intent)
        controller.get().activityResultImplementation(activityResult)

        assertTrue(activity.isFinishing)
    }

    @Test
    fun `process intent with speech processing set to false`() {
        allowVoiceIntentToResolveActivity()
        val intent = Intent()
        intent.putExtra(SPEECH_PROCESSING, false)

        val controller = Robolectric.buildActivity(BaseVoiceSearchActivityExtendedForTests::class.java, intent)
        val activity = controller.get()

        controller.create()

        assertTrue(activity.isFinishing)
    }

    @Test
    fun `process null intent`() {
        allowVoiceIntentToResolveActivity()
        val controller = Robolectric.buildActivity(BaseVoiceSearchActivityExtendedForTests::class.java, null)
        val activity = controller.get()

        controller.create()

        assertTrue(activity.isFinishing)
    }

    @Test
    fun `save previous intent to instance state`() {
        allowVoiceIntentToResolveActivity()
        val previousIntent = Intent().apply {
            putExtra(SPEECH_PROCESSING, true)
        }
        val savedInstanceState = Bundle().apply {
            putParcelable(PREVIOUS_INTENT, previousIntent)
        }
        val outState = Bundle()

        controller.create(savedInstanceState)
        controller.saveInstanceState(outState)

        assertEquals(previousIntent, outState.getParcelable<Intent>(PREVIOUS_INTENT))
    }

    @Test
    fun `process intent with speech processing in previous intent set to true`() {
        allowVoiceIntentToResolveActivity()
        val savedInstanceState = Bundle()
        val previousIntent = Intent().apply {
            putExtra(SPEECH_PROCESSING, true)
        }
        savedInstanceState.putParcelable(PREVIOUS_INTENT, previousIntent)

        controller.create(savedInstanceState)

        assertFalse(activity.isFinishing)
        assertNull(shadow.peekNextStartedActivityForResult())
    }

    @Test
    fun `handle invalid result code`() {
        val activityResult = ActivityResult(Activity.RESULT_CANCELED, Intent())
        controller.get().activityResultImplementation(activityResult)

        assertTrue(activity.isFinishing)
    }
}
