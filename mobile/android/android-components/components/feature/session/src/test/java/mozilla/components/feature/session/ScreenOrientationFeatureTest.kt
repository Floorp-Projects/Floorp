/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verify

class ScreenOrientationFeatureTest {
    @Test
    fun `WHEN the feature starts THEN register itself as a screen orientation delegate`() {
        val engine = mock<Engine>()
        val feature = ScreenOrientationFeature(engine, mock())

        feature.start()

        verify(engine).registerScreenOrientationDelegate(feature)
    }

    @Test
    fun `WHEN the feature stops THEN unregister itself as the screen orientation delegate`() {
        val engine = mock<Engine>()
        val feature = ScreenOrientationFeature(engine, mock())

        feature.stop()

        verify(engine).unregisterScreenOrientationDelegate()
    }

    @Test
    fun `WHEN asked to set a screen orientation THEN set it on the activity property and return true`() {
        val activity = mock<Activity>()
        val feature = ScreenOrientationFeature(mock(), activity)

        val result = feature.onOrientationLock(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)

        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        assertTrue(result)
    }

    @Test
    fun `WHEN asked to reset screen orientation THEN set it to UNSPECIFIED`() {
        val activity = mock<Activity>()
        val feature = ScreenOrientationFeature(mock(), activity)

        feature.onOrientationUnlock()

        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }
}
