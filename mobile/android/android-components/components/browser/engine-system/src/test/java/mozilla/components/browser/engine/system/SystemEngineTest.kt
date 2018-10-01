/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import mozilla.components.concept.engine.DefaultSettings
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineTest {

    private val context = RuntimeEnvironment.application

    @Before
    fun setup() {
        // This is setting a internal field just for testing purposes as
        // WebSettings.getDefaultUserAgent isn't mocked by Roboelectric
        SystemEngine.defaultUserAgent = "test-ua-string"
    }

    @Test
    fun testCreateView() {
        val engine = SystemEngine(context)
        assertTrue(engine.createView(context) is SystemEngineView)
    }

    @Test
    fun testCreateSession() {
        val engine = SystemEngine(context)
        assertTrue(engine.createSession() is SystemEngineSession)

        try {
            engine.createSession(true)
            // Private browsing not yet supported
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedOperationException) { }
    }

    @Test
    fun testName() {
        val engine = SystemEngine(context)
        assertEquals("System", engine.name())
    }

    @Test
    fun testSettings() {
        var engine = SystemEngine(context, DefaultSettings(remoteDebuggingEnabled = true))
        assertTrue(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = false
        assertFalse(engine.settings.remoteDebuggingEnabled)

        // Specifying no ua-string default should result in WebView default
        // It should be possible to read and set a new default
        assertEquals("test-ua-string", engine.settings.userAgentString)
        engine.settings.userAgentString = engine.settings.userAgentString + "-test"
        assertEquals("test-ua-string-test", engine.settings.userAgentString)

        // It should be possible to specify a custom ua-string default
        assertEquals("foo", SystemEngine(context, DefaultSettings(userAgentString = "foo")).settings.userAgentString)
    }
}