/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.extensions

import android.content.Context
import mozilla.components.concept.engine.EngineSession
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.HardcodedNimbusFeatures
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.gecko.GeckoProvider
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.nimbus.FxNimbus

/**
 * Instrumentation test for verifying that the extensions process can be controlled with Nimbus.
 */
class ExtensionProcessTest {
    private lateinit var context: Context
    private lateinit var policy: EngineSession.TrackingProtectionPolicy

    @Before
    fun setUp() {
        context = TestHelper.appContext
        policy =
            context.components.core.trackingProtectionPolicyFactory.createTrackingProtectionPolicy()
    }

    @Test
    fun test_extension_process_can_be_controlled_by_nimbus() {
        val hardcodedNimbus = HardcodedNimbusFeatures(
            context,
            "extensions-process" to JSONObject(
                """
                  {
                    "enabled":true
                  }
                """.trimIndent(),
            ),
        )

        hardcodedNimbus.connectWith(FxNimbus)

        val runtime = GeckoProvider.createRuntimeSettings(context, policy)

        assertTrue(FxNimbus.features.extensionsProcess.value().enabled)
        assertTrue(runtime.extensionsProcessEnabled!!)
    }

    @Test
    fun test_extension_process_must_be_disabled_by_default() {
        val runtime = GeckoProvider.createRuntimeSettings(context, policy)

        assertFalse(FxNimbus.features.extensionsProcess.value().enabled)
        assertFalse(runtime.extensionsProcessEnabled!!)
    }
}
