/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NimbusAppInfo
import org.mozilla.experiments.nimbus.NimbusInterface

@RunWith(AndroidJUnit4::class)
class NimbusTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private val appInfo = NimbusAppInfo(
        appName = "NimbusUnitTest",
        channel = "test",
    )

    @Test
    fun `Nimbus disabled and enabled can have observers registered on it`() {
        val enabled: NimbusApi = Nimbus(context, appInfo, null)
        val disabled: NimbusApi = NimbusDisabled(context)

        val observer = object : NimbusInterface.Observer {}

        enabled.register(observer)
        disabled.register(observer)
    }

    @Test
    fun `NimbusDisabled is empty`() {
        val nimbus: NimbusApi = NimbusDisabled(context)
        nimbus.fetchExperiments()
        nimbus.applyPendingExperiments()
        assertTrue("getActiveExperiments should be empty", nimbus.getActiveExperiments().isEmpty())
        assertEquals(null, nimbus.getExperimentBranch("test-experiment"))
    }
}
