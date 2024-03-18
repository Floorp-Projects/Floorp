/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.experiment

import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.experiment.NimbusExperimentDelegate
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.hamcrest.CoreMatchers.equalTo
import org.hamcrest.MatcherAssert.assertThat
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.ExperimentDelegate
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class NimbusExperimentDelegateTest {

    @Test
    fun `WHEN an experiment delegate can be set on the runtime THEN the same delegate can be returned`() {
        val runtime = mock<GeckoRuntime>()
        val mockExperimentDelegate = mock<ExperimentDelegate>()
        val mockGeckoSetting = mock<GeckoRuntimeSettings>()
        whenever(runtime.settings).thenReturn(mockGeckoSetting)
        whenever(mockGeckoSetting.experimentDelegate).thenReturn(mockExperimentDelegate)
        assertThat("Can set and retrieve experiment delegate.", runtime.settings.experimentDelegate, equalTo(mockExperimentDelegate))
    }

    @Test
    fun `WHEN the Nimbus experiment delegate is used AND the feature does not exist THEN the delegate responds with exceptions`() {
        val nimbusExperimentDelegate = NimbusExperimentDelegate()
        nimbusExperimentDelegate.onGetExperimentFeature("test-no-op")
            .accept { assertTrue("Should not have completed.", false) }
            .exceptionally { e ->
                assertTrue("Should have completed exceptionally.", (e as ExperimentException).code == ExperimentException.ERROR_FEATURE_NOT_FOUND)
                GeckoResult.fromValue(null)
            }

        nimbusExperimentDelegate.onRecordExposureEvent("test-no-op")
            .accept { assertTrue("Should not have completed.", false) }
            .exceptionally { e ->
                assertTrue("Should have completed exceptionally.", (e as ExperimentException).code == ExperimentException.ERROR_FEATURE_NOT_FOUND)
                GeckoResult.fromValue(null)
            }
        nimbusExperimentDelegate.onRecordExperimentExposureEvent("test-no-op", "test-no-op")
            .accept { assertTrue("Should not have completed.", false) }
            .exceptionally { e ->
                assertTrue("Should have completed exceptionally.", (e as ExperimentException).code == ExperimentException.ERROR_FEATURE_NOT_FOUND)
                GeckoResult.fromValue(null)
            }
        nimbusExperimentDelegate.onRecordMalformedConfigurationEvent("test-no-op", "test")
            .accept { assertTrue("Should not have completed.", false) }
            .exceptionally { e ->
                assertTrue("Should have completed exceptionally.", (e as ExperimentException).code == ExperimentException.ERROR_FEATURE_NOT_FOUND)
                GeckoResult.fromValue(null)
            }

        shadowOf(getMainLooper()).idle()
    }
}
