/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.extensions

import android.content.Context
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.gecko.GeckoProvider
import org.mozilla.fenix.helpers.TestHelper

/**
 * Instrumentation test for verifying that the extensions process is enabled unconditionally.
 */
class ExtensionProcessTest {
    private lateinit var context: Context
    private lateinit var policy: EngineSession.TrackingProtectionPolicy

    @Before
    fun setUp() {
        context = TestHelper.appContext
        policy = context.components.core.trackingProtectionPolicyFactory.createTrackingProtectionPolicy()
    }

    @Test
    fun test_extension_process_is_enabled() {
        val runtime = GeckoProvider.createRuntimeSettings(context, policy)
        assertTrue(runtime.extensionsProcessEnabled!!)
    }
}
