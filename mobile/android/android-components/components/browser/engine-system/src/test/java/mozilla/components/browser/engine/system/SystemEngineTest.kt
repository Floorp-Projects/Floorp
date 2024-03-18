/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SystemEngineTest {

    @Before
    fun setup() {
        // This is setting a internal field just for testing purposes as
        // WebSettings.getDefaultUserAgent isn't mocked by Roboelectric
        SystemEngine.defaultUserAgent = "test-ua-string"
    }

    @Test
    fun createView() {
        val engine = SystemEngine(testContext)
        assertTrue(engine.createView(testContext) is SystemEngineView)
    }

    @Test
    fun createSession() {
        val engine = SystemEngine(testContext)
        assertTrue(engine.createSession() is SystemEngineSession)

        try {
            engine.createSession(true)
            // Private browsing not yet supported
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedOperationException) { }

        try {
            engine.createSession(false, "1")
            // Contextual identities not yet supported
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedOperationException) { }
    }

    @Test
    fun name() {
        val engine = SystemEngine(testContext)
        assertEquals("System", engine.name())
    }

    @Test
    fun settings() {
        val engine = SystemEngine(
            testContext,
            DefaultSettings(
                remoteDebuggingEnabled = true,
                trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.strict(),
            ),
        )

        assertTrue(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = false
        assertFalse(engine.settings.remoteDebuggingEnabled)

        assertEquals(
            engine.settings.trackingProtectionPolicy,
            EngineSession.TrackingProtectionPolicy.strict(),
        )
        engine.settings.trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.none()
        assertEquals(engine.settings.trackingProtectionPolicy, EngineSession.TrackingProtectionPolicy.none())

        // Specifying no ua-string default should result in WebView default
        // It should be possible to read and set a new default
        assertEquals("test-ua-string", engine.settings.userAgentString)
        engine.settings.userAgentString = engine.settings.userAgentString + "-test"
        assertEquals("test-ua-string-test", engine.settings.userAgentString)

        // It should be possible to specify a custom ua-string default
        assertEquals("foo", SystemEngine(testContext, DefaultSettings(userAgentString = "foo")).settings.userAgentString)
    }

    // This feature will be covered on this issue
    // https://github.com/mozilla-mobile/android-components/issues/4206
    @Test(expected = UnsupportedSettingException::class)
    fun safeBrowsingIsNotSupportedYet() {
        val engine = SystemEngine(
            testContext,
            DefaultSettings(
                remoteDebuggingEnabled = true,
                trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.strict(),
            ),
        )

        engine.settings.safeBrowsingPolicy
    }
}
