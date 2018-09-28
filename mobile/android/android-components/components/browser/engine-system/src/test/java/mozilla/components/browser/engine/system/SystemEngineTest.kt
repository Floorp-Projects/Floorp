/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import mozilla.components.concept.engine.DefaultSettings
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineTest {

    @Test
    fun testCreateView() {
        assertTrue(SystemEngine().createView(RuntimeEnvironment.application) is SystemEngineView)
    }

    @Test
    fun testCreateSession() {
        assertTrue(SystemEngine().createSession() is SystemEngineSession)

        try {
            SystemEngine().createSession(true)
            // Private browsing not yet supported
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedOperationException) { }
    }

    @Test
    fun testName() {
        assertEquals("System", SystemEngine().name())
    }

    @Test
    fun testSettings() {
        val engine = SystemEngine(DefaultSettings(remoteDebuggingEnabled = true))
        assertTrue(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = false
        assertFalse(engine.settings.remoteDebuggingEnabled)
    }
}