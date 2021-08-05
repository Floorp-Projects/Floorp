/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.ServiceResult
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
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

class UtilsKtTest {
    @Test
    fun `handleFxaExceptions form 1 returns correct data back`() = runBlocking {
        assertEquals(
            1,
            handleFxaExceptions(
                mock(), "test op",
                {
                    1
                },
                { fail() }, { fail() }
            )
        )

        assertEquals(
            "Hello",
            handleFxaExceptions(
                mock(), "test op",
                {
                    "Hello"
                },
                { fail() }, { fail() }
            )
        )
    }

    @Test
    fun `handleFxaExceptions form 1 does not swallow non-panics`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        // Network.
        assertEquals(
            "pass!",
            handleFxaExceptions(
                mock(), "test op",
                {
                    throw FxaNetworkException("oops")
                },
                { "fail" },
                { error ->
                    assertEquals("oops", error.message)
                    assertTrue(error is FxaNetworkException)
                    "pass!"
                }
            )
        )

        verifyNoInteractions(accountManager)

        assertEquals(
            "pass!",
            handleFxaExceptions(
                mock(), "test op",
                {
                    throw FxaUnauthorizedException("auth!")
                },
                {
                    "pass!"
                },
                {
                    fail()
                }
            )
        )

        verify(accountManager).encounteredAuthError(eq("test op"), anyInt())

        reset(accountManager)
        assertEquals(
            "pass!",
            handleFxaExceptions(
                mock(), "test op",
                {
                    throw FxaUnspecifiedException("dunno")
                },
                { "fail" },
                { error ->
                    assertEquals("dunno", error.message)
                    assertTrue(error is FxaUnspecifiedException)
                    "pass!"
                }
            )
        )
        verifyNoInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 1 re-throws non-fxa exceptions`() = runBlocking {
        handleFxaExceptions(
            mock(), "test op",
            {
                throw IllegalStateException("bad state")
            },
            { fail() }, { fail() }
        )
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 1 re-throws fxa panic exceptions`() = runBlocking {
        handleFxaExceptions(
            mock(), "test op",
            {
                throw FxaPanicException("don't panic!")
            },
            { fail() }, { fail() }
        )
    }

    @Test
    fun `handleFxaExceptions form 2 works`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        assertTrue(
            handleFxaExceptions(mock(), "test op") {
                Unit
            }
        )

        assertFalse(
            handleFxaExceptions(mock(), "test op") {
                throw FxaUnspecifiedException("dunno")
            }
        )

        verifyNoInteractions(accountManager)

        assertFalse(
            handleFxaExceptions(mock(), "test op") {
                throw FxaUnauthorizedException("401")
            }
        )

        verify(accountManager).encounteredAuthError("test op")

        reset(accountManager)

        assertFalse(
            handleFxaExceptions(mock(), "test op") {
                throw FxaNetworkException("dunno")
            }
        )

        verifyNoInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 2 re-throws non-fxa exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op") {
            throw IllegalStateException("bad state")
        }
        verifyNoInteractions(accountManager)
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 2 re-throws fxa panic exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op") {
            throw FxaPanicException("dunno")
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `handleFxaExceptions form 3 works`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        assertEquals(
            1,
            handleFxaExceptions(mock(), "test op", { 2 }) {
                1
            }
        )

        assertEquals(
            0,
            handleFxaExceptions(mock(), "test op", { 0 }) {
                throw FxaUnspecifiedException("dunno")
            }
        )

        verifyNoInteractions(accountManager)

        assertEquals(
            -1,
            handleFxaExceptions(mock(), "test op", { -1 }) {
                throw FxaUnauthorizedException("401")
            }
        )

        verify(accountManager).encounteredAuthError(eq("test op"), anyInt())

        reset(accountManager)

        assertEquals(
            "bad",
            handleFxaExceptions(mock(), "test op", { "bad" }) {
                throw FxaNetworkException("dunno")
            }
        )

        verifyNoInteractions(accountManager)
    }

    @Test(expected = IllegalStateException::class)
    fun `handleFxaExceptions form 3 re-throws non-fxa exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw IllegalStateException("bad state")
        }
        verifyNoInteractions(accountManager)
    }

    @Test(expected = FxaPanicException::class)
    fun `handleFxaExceptions form 3 re-throws fxa panic exceptions`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(accountManager)

        handleFxaExceptions(mock(), "test op", { "nope" }) {
            throw FxaPanicException("dunno")
        }
        verifyNoInteractions(accountManager)
    }

    @Test
    fun `withRetries immediate success`() = runBlocking {
        when (val res = withRetries(mock(), 3) { true }) {
            is Result.Success -> assertTrue(res.value)
            is Result.Failure -> fail()
        }
        when (val res = withRetries(mock(), 3) { "hello!" }) {
            is Result.Success -> assertEquals("hello!", res.value)
            is Result.Failure -> fail()
        }
        val eventual = SucceedOn(2, 42)
        when (val res = withRetries(mock(), 3) { eventual.nullFailure() }) {
            is Result.Success -> assertEquals(42, res.value)
            is Result.Failure -> fail()
        }
    }

    @Test
    fun `withRetries immediate failure`() = runBlocking {
        when (withRetries(mock(), 3) { false }) {
            is Result.Success -> fail()
            is Result.Failure -> {}
        }
        when (withRetries(mock(), 3) { null }) {
            is Result.Success -> fail()
            is Result.Failure -> {}
        }
    }

    @Test
    fun `withRetries eventual success`() = runBlocking {
        val eventual = SucceedOn(2, true)
        when (val res = withRetries(mock(), 5) { eventual.nullFailure() }) {
            is Result.Success -> {
                assertTrue(res.value!!)
                assertEquals(2, eventual.attempts)
            }
            is Result.Failure -> fail()
        }
        val eventual2 = SucceedOn(2, "world!")
        when (val res = withRetries(mock(), 3) { eventual2.nullFailure() }) {
            is Result.Success -> {
                assertEquals("world!", res.value)
                assertEquals(2, eventual2.attempts)
            }
            is Result.Failure -> fail()
        }
    }

    @Test
    fun `withRetries eventual failure`() = runBlocking {
        val eventual = SucceedOn(6, true)
        when (withRetries(mock(), 5) { eventual.nullFailure() }) {
            is Result.Success -> fail()
            is Result.Failure -> {
                assertEquals(5, eventual.attempts)
            }
        }
        val eventual2 = SucceedOn(15, "hello!")
        when (withRetries(mock(), 3) { eventual2.nullFailure() }) {
            is Result.Success -> fail()
            is Result.Failure -> {
                assertEquals(3, eventual2.attempts)
            }
        }
    }

    @Test
    fun `withServiceRetries immediate success`() = runBlocking {
        when (withServiceRetries(mock(), 3, suspend { ServiceResult.Ok })) {
            is ServiceResult.Ok -> {}
            else -> fail()
        }
    }

    @Test
    fun `withServiceRetries generic failure keeps retrying`() = runBlocking {
        // keeps retrying on generic error
        val eventual = SucceedOn(0, ServiceResult.Ok, ServiceResult.OtherError)
        when (withServiceRetries(mock(), 3) { eventual.reifiedFailure() }) {
            is ServiceResult.Ok -> fail()
            else -> {
                assertEquals(3, eventual.attempts)
            }
        }
    }

    @Test
    fun `withServiceRetries auth failure short circuit`() = runBlocking {
        // keeps retrying on generic error
        val eventual = SucceedOn(0, ServiceResult.Ok, ServiceResult.AuthError)
        when (withServiceRetries(mock(), 3) { eventual.reifiedFailure() }) {
            is ServiceResult.Ok -> fail()
            else -> {
                assertEquals(1, eventual.attempts)
            }
        }
    }

    @Test
    fun `withServiceRetries eventual success`() = runBlocking {
        val eventual = SucceedOn(3, ServiceResult.Ok, ServiceResult.OtherError)
        when (withServiceRetries(mock(), 5) { eventual.reifiedFailure() }) {
            is ServiceResult.Ok -> {
                assertEquals(3, eventual.attempts)
            }
            else -> fail()
        }
    }

    @Test
    fun `as auth flow pairing`() = runBlocking {
        val account: OAuthAccount = mock()
        val authFlowUrl: AuthFlowUrl = mock()
        `when`(account.beginPairingFlow(eq("http://pairing.url"), eq(emptySet()), anyString())).thenReturn(authFlowUrl)
        verify(account, never()).beginOAuthFlow(eq(emptySet()), anyString())
        assertEquals(authFlowUrl, "http://pairing.url".asAuthFlowUrl(account, emptySet()))
    }

    @Test
    fun `as auth flow regular`() = runBlocking {
        val account: OAuthAccount = mock()
        val authFlowUrl: AuthFlowUrl = mock()
        `when`(account.beginOAuthFlow(eq(emptySet()), anyString())).thenReturn(authFlowUrl)
        verify(account, never()).beginPairingFlow(anyString(), eq(emptySet()), anyString())
        assertEquals(authFlowUrl, null.asAuthFlowUrl(account, emptySet()))
    }

    private class SucceedOn<S>(private val successOn: Int, private val succeedWith: S, private val failWith: S? = null) {
        var attempts = 0
        fun nullFailure(): S? {
            attempts += 1
            return when {
                successOn == 0 || attempts < successOn -> failWith
                else -> succeedWith!!
            }
        }
        fun reifiedFailure(): S = nullFailure()!!
    }
}
