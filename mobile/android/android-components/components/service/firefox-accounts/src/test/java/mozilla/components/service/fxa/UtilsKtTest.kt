/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.runBlocking
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.GlobalAccountManager
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions

class UtilsKtTest {
    @Test
    fun `handleFxaExceptions form 1 returns correct data back`() = runBlocking {
        assertEquals(1, handleFxaExceptions(mock(), "test op", {
            1
        }, { fail() }, { fail() }))

        assertEquals("Hello", handleFxaExceptions(mock(), "test op", {
            "Hello"
        }, { fail() }, { fail() }))
    }

    @Test
    fun `handleFxaExceptions form 1 does not swallow non-panics`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        // Network.
        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaNetworkException("oops")
        }, { "fail" }, { error ->
            assertEquals("oops", error.message)
            assertTrue(error is FxaNetworkException)
            "pass!"
        }))

        verifyZeroInteractions(accountManager)

        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaUnauthorizedException("auth!")
        }, {
            "pass!"
        }, {
            fail()
        }))

        verify(accountManager).encounteredAuthError(eq("test op"), anyInt())

        reset(accountManager)
        assertEquals("pass!", handleFxaExceptions(mock(), "test op", {
            throw FxaUnspecifiedException("dunno")
        }, { "fail" }, { error ->
            assertEquals("dunno", error.message)
            assertTrue(error is FxaUnspecifiedException)
            "pass!"
        }))
        verifyZeroInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 1 re-throws non-fxa exceptions`() = runBlocking {
        handleFxaExceptions(mock(), "test op", {
            throw IllegalStateException("bad state")
        }, { fail() }, { fail() })
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 1 re-throws fxa panic exceptions`() = runBlocking {
        handleFxaExceptions(mock(), "test op", {
            throw FxaPanicException("don't panic!")
        }, { fail() }, { fail() })
    }

    @Test
    fun `handleFxaExceptions form 2 works`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        assertTrue(handleFxaExceptions(mock(), "test op") {
            Unit
        })

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaUnspecifiedException("dunno")
        })

        verifyZeroInteractions(accountManager)

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaUnauthorizedException("401")
        })

        verify(accountManager).encounteredAuthError("test op")

        reset(accountManager)

        assertFalse(handleFxaExceptions(mock(), "test op") {
            throw FxaNetworkException("dunno")
        })

        verifyZeroInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 2 re-throws non-fxa exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op") {
            throw IllegalStateException("bad state")
        }
        verifyZeroInteractions(accountManager)
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 2 re-throws fxa panic exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op") {
            throw FxaPanicException("dunno")
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `handleFxaExceptions form 3 works`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        assertEquals(1, handleFxaExceptions(mock(), "test op", { 2 }) {
            1
        })

        assertEquals(0, handleFxaExceptions(mock(), "test op", { 0 }) {
            throw FxaUnspecifiedException("dunno")
        })

        verifyZeroInteractions(accountManager)

        assertEquals(-1, handleFxaExceptions(mock(), "test op", { -1 }) {
            throw FxaUnauthorizedException("401")
        })

        verify(accountManager).encounteredAuthError(eq("test op"), anyInt())

        reset(accountManager)

        assertEquals("bad", handleFxaExceptions(mock(), "test op", { "bad" }) {
            throw FxaNetworkException("dunno")
        })

        verifyZeroInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 3 re-throws non-fxa exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw IllegalStateException("bad state")
        }
        verifyZeroInteractions(accountManager)
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 3 re-throws fxa panic exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw FxaPanicException("dunno")
        }
        verifyZeroInteractions(accountManager)
    }
}