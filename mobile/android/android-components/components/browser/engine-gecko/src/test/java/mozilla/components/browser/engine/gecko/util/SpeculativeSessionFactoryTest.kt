/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.util

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntime

@RunWith(AndroidJUnit4::class)
class SpeculativeSessionFactoryTest {

    private lateinit var runtime: GeckoRuntime

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
    }

    @Test
    fun `create does nothing if matching speculative session already exists`() {
        val factory = SpeculativeSessionFactory()
        assertNull(factory.speculativeEngineSession)

        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        val speculativeSession = factory.speculativeEngineSession
        assertNotNull(speculativeSession)

        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        assertSame(speculativeSession, factory.speculativeEngineSession)
    }

    @Test
    fun `create clears previous non-matching speculative session`() {
        val factory = SpeculativeSessionFactory()
        assertNull(factory.speculativeEngineSession)

        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        val speculativeSession = factory.speculativeEngineSession
        assertNotNull(speculativeSession)

        factory.create(runtime = runtime, private = false, contextId = null, defaultSettings = mock())
        assertNotSame(speculativeSession, factory.speculativeEngineSession)
        assertFalse(speculativeSession!!.engineSession.geckoSession.isOpen)
        assertFalse(speculativeSession.engineSession.isObserved())
    }

    @Test
    fun `get consumes matching speculative session`() {
        val factory = SpeculativeSessionFactory()
        assertFalse(factory.hasSpeculativeSession())

        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        assertTrue(factory.hasSpeculativeSession())

        val speculativeSession = factory.get(private = true, contextId = null)
        assertNotNull(speculativeSession)
        assertFalse(speculativeSession!!.isObserved())

        assertFalse(factory.hasSpeculativeSession())
        assertNull(factory.speculativeEngineSession)
    }

    @Test
    fun `get clears previous non-matching speculative session`() {
        val factory = SpeculativeSessionFactory()
        assertNull(factory.speculativeEngineSession)

        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        val speculativeSession = factory.speculativeEngineSession
        assertNotNull(speculativeSession)

        assertNull(factory.get(private = true, contextId = "test"))
        assertFalse(speculativeSession!!.engineSession.geckoSession.isOpen)
        assertFalse(speculativeSession.engineSession.isObserved())
    }

    @Test
    fun `clears speculative session on crash`() {
        val factory = SpeculativeSessionFactory()
        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        assertTrue(factory.hasSpeculativeSession())
        val speculativeSession = factory.speculativeEngineSession

        factory.speculativeEngineSession?.engineSession?.notifyObservers { onCrash() }
        assertFalse(factory.hasSpeculativeSession())
        assertFalse(speculativeSession!!.engineSession.geckoSession.isOpen)
        assertFalse(speculativeSession.engineSession.isObserved())
    }

    @Test
    fun `clears speculative session when process is killed`() {
        val factory = SpeculativeSessionFactory()
        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        assertTrue(factory.hasSpeculativeSession())
        val speculativeSession = factory.speculativeEngineSession

        factory.speculativeEngineSession?.engineSession?.notifyObservers { onProcessKilled() }
        assertFalse(factory.hasSpeculativeSession())
        assertFalse(speculativeSession!!.engineSession.geckoSession.isOpen)
        assertFalse(speculativeSession.engineSession.isObserved())
    }

    @Test
    fun `clear unregisters observer and closes session`() {
        val factory = SpeculativeSessionFactory()
        factory.create(runtime = runtime, private = true, contextId = null, defaultSettings = mock())
        assertTrue(factory.hasSpeculativeSession())
        val speculativeSession = factory.speculativeEngineSession
        assertTrue(speculativeSession!!.engineSession.isObserved())

        factory.clear()
        assertFalse(factory.hasSpeculativeSession())
        assertFalse(speculativeSession.engineSession.geckoSession.isOpen)
        assertFalse(speculativeSession.engineSession.isObserved())
    }
}
