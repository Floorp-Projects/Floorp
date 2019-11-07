/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verifyZeroInteractions
import org.mockito.Mockito.verify
import org.mozilla.fenix.TestApplication
import org.robolectric.annotation.Config
import kotlin.reflect.KClass

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class AccountAbnormalitiesTest {
    @Test
    fun `account manager must be configured`() {
        val crashReporter: CrashReporter = mock()

        // no account present
        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter)

        try {
            accountAbnormalities.userRequestedLogout()
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("userRequestedLogout before account manager was configured", e.message)
        }

        try {
            accountAbnormalities.onAuthenticated(mock(), mock())
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("onAuthenticated before account manager was configured", e.message)
        }

        try {
            accountAbnormalities.onLoggedOut()
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("onLoggedOut before account manager was configured", e.message)
        }

        verifyZeroInteractions(crashReporter)
    }

    @Test
    fun `LogoutWithoutAuth detected`() = runBlocking {
        val crashReporter: CrashReporter = mock()
        val accountManager: FxaAccountManager = mock()

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // Logout action must be preceded by auth.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException(crashReporter, AbnormalFxaEvent.LogoutWithoutAuth::class)
    }

    @Test
    fun `OverlappingFxaLogoutRequest detected`() = runBlocking {
        val crashReporter: CrashReporter = mock()
        val accountManager: FxaAccountManager = mock()

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        accountAbnormalities.onAuthenticated(mock(), mock())
        // So far, so good. A regular logout request while being authenticated.
        accountAbnormalities.userRequestedLogout()
        verifyZeroInteractions(crashReporter)

        // We never saw a logout callback after previous logout request, so this is an overlapping request.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException(crashReporter, AbnormalFxaEvent.OverlappingFxaLogoutRequest::class)
    }

    @Test
    fun `callback logout abnormalities detected`() = runBlocking {
        val crashReporter: CrashReporter = mock()
        val accountManager: FxaAccountManager = mock()

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // User didn't request this logout.
        accountAbnormalities.onLoggedOut()
        assertCaughtException(crashReporter, AbnormalFxaEvent.UnexpectedFxaLogout::class)
    }

    @Test
    fun `login happy case + disappearing account detected`() = runBlocking {
        val crashReporter: CrashReporter = mock()
        val accountManager: FxaAccountManager = mock()

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        accountAbnormalities.onAuthenticated(mock(), mock())
        verifyZeroInteractions(crashReporter)

        // Pretend we restart, and instantiate a new middleware instance.
        val accountAbnormalities2 = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        // mock accountManager doesn't have an account, but we expect it to have one since we
        // were authenticated before our "restart".
        accountAbnormalities2.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        assertCaughtException(crashReporter, AbnormalFxaEvent.MissingExpectedAccountAfterStartup::class)
    }

    @Test
    fun `logout happy case`() = runBlocking {
        val crashReporter: CrashReporter = mock()
        val accountManager: FxaAccountManager = mock()

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // We saw an auth event, then user requested a logout.
        accountAbnormalities.onAuthenticated(mock(), mock())
        accountAbnormalities.userRequestedLogout()
        verifyZeroInteractions(crashReporter)
    }

    private fun <T : AbnormalFxaEvent> assertCaughtException(crashReporter: CrashReporter, type: KClass<T>) {
        val captor = argumentCaptor<AbnormalFxaEvent>()
        verify(crashReporter).submitCaughtException(captor.capture())
        assertEquals(type, captor.value::class)
    }
}
