/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import mozilla.components.concept.sync.AuthException
import mozilla.components.service.fxa.manager.AuthErrorObserver
import mozilla.components.service.fxa.manager.authErrorRegistry
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test

class UtilsKtTest {
    private class TestAuthErrorObserver : AuthErrorObserver {
        var lastError: AuthException? = null

        override fun onAuthErrorAsync(e: AuthException): Deferred<Unit> {
            lastError = e
            // unit completable starts off in a non-active state.
            val done = CompletableDeferred<Unit>()
            done.complete(Unit)
            return done
        }
    }

    @Before
    fun cleanup() {
        authErrorRegistry.unregisterObservers()
    }

    @Test
    fun `handleFxaExceptions form 1 returns correct data back`() {
        assertEquals(1, handleFxaExceptions(mock(), "test op", {
            1
        }, { fail() }, { fail() }))

        assertEquals("Hello", handleFxaExceptions(mock(), "test op", {
            "Hello"
        }, { fail() }, { fail() }))
    }

    @Test
    fun `handleFxaExceptions form 1 does not swallow non-panics`() {
        val authErrorObserver = TestAuthErrorObserver()
        authErrorRegistry.register(authErrorObserver)

        // Network.
        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaNetworkException("oops")
        }, { "fail" }, { error ->
            assertEquals("oops", error.message)
            assertTrue(error is FxaNetworkException)
            "pass!"
        }))

        assertNull(authErrorObserver.lastError)

        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaUnauthorizedException("auth!")
        }, {
            "pass!"
        }, {
            fail()
        }))

        assertEquals("auth!", (authErrorObserver.lastError as AuthException).cause!!.message)
        assertTrue((authErrorObserver.lastError as AuthException).cause is FxaUnauthorizedException)

        authErrorObserver.lastError = null
        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaUnspecifiedException("dunno")
        }, { "fail" }, { error ->
            assertEquals("dunno", error.message)
            assertTrue(error is FxaUnspecifiedException)
            "pass!"
        }))
        assertNull(authErrorObserver.lastError)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 1 re-throws non-fxa exceptions`() {
        handleFxaExceptions(mock(), "test op", {
            throw IllegalStateException("bad state")
        }, { fail() }, { fail() })
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 1 re-throws fxa panic exceptions`() {
        handleFxaExceptions(mock(), "test op", {
            throw FxaPanicException("don't panic!")
        }, { fail() }, { fail() })
    }

    @Test
    fun `handleFxaExceptions form 2 works`() {
        val authErrorObserver = TestAuthErrorObserver()
        authErrorRegistry.register(authErrorObserver)

        assertTrue(handleFxaExceptions(mock(), "test op") {
            Unit
        })

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaUnspecifiedException("dunno")
        })

        assertNull(authErrorObserver.lastError)

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaUnauthorizedException("401")
        })

        assertEquals("401", (authErrorObserver.lastError as AuthException).cause!!.message)
        assertTrue((authErrorObserver.lastError as AuthException).cause is FxaUnauthorizedException)

        authErrorObserver.lastError = null

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaNetworkException("dunno")
        })

        assertNull(authErrorObserver.lastError)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 2 re-throws non-fxa exceptions`() {
        handleFxaExceptions(mock(), "test op") {
            throw IllegalStateException("bad state")
        }
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 2 re-throws fxa panic exceptions`() {
        handleFxaExceptions(mock(), "test op") {
            throw FxaPanicException("dunno")
        }
    }

    @Test
    fun `handleFxaExceptions form 3 works`() {
        val authErrorObserver = TestAuthErrorObserver()
        authErrorRegistry.register(authErrorObserver)

        assertEquals(1, handleFxaExceptions(mock(), "test op", { 2 }) {
            1
        })

        assertEquals(0, handleFxaExceptions(mock(), "test op", { 0 }) {
            throw FxaUnspecifiedException("dunno")
        })

        assertNull(authErrorObserver.lastError)

        assertEquals(-1, handleFxaExceptions(mock(), "test op", { -1 }) {
            throw FxaUnauthorizedException("401")
        })

        assertEquals("401", (authErrorObserver.lastError as AuthException).cause!!.message)
        assertTrue((authErrorObserver.lastError as AuthException).cause is FxaUnauthorizedException)

        authErrorObserver.lastError = null

        assertEquals("bad", handleFxaExceptions(mock(), "test op", { "bad" }) {
            throw FxaNetworkException("dunno")
        })

        assertNull(authErrorObserver.lastError)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 3 re-throws non-fxa exceptions`() {
        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw IllegalStateException("bad state")
        }
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 3 re-throws fxa panic exceptions`() {
        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw FxaPanicException("dunno")
        }
    }
}