/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(FenixRobolectricTestRunner::class)
class AccountAbnormalitiesTest {
    @Test
    fun `account manager must be configured`() {
        val crashReporter: CrashReporter = mockk()

        // no account present
        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter)

        try {
            accountAbnormalities.userRequestedLogout()
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("userRequestedLogout before account manager was configured", e.message)
        }

        try {
            accountAbnormalities.onAuthenticated(mockk(), mockk())
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

        verify { crashReporter wasNot Called }
    }

    @Test
    fun `LogoutWithoutAuth detected`() = runBlocking {
        val crashReporter: CrashReporter = mockk(relaxed = true)
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // Logout action must be preceded by auth.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException<AbnormalFxaEvent.LogoutWithoutAuth>(crashReporter)
    }

    @Test
    fun `OverlappingFxaLogoutRequest detected`() = runBlocking {
        val crashReporter: CrashReporter = mockk(relaxed = true)
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        accountAbnormalities.onAuthenticated(mockk(), mockk())
        // So far, so good. A regular logout request while being authenticated.
        accountAbnormalities.userRequestedLogout()
        verify { crashReporter wasNot Called }

        // We never saw a logout callback after previous logout request, so this is an overlapping request.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException<AbnormalFxaEvent.OverlappingFxaLogoutRequest>(crashReporter)
    }

    @Test
    fun `callback logout abnormalities detected`() = runBlocking {
        val crashReporter: CrashReporter = mockk(relaxed = true)
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // User didn't request this logout.
        accountAbnormalities.onLoggedOut()
        assertCaughtException<AbnormalFxaEvent.UnexpectedFxaLogout>(crashReporter)
    }

    @Test
    fun `login happy case + disappearing account detected`() = runBlocking {
        val crashReporter: CrashReporter = mockk(relaxed = true)
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        accountAbnormalities.onAuthenticated(mockk(), mockk())
        verify { crashReporter wasNot Called }
        every { accountManager.authenticatedAccount() } returns null

        // Pretend we restart, and instantiate a new middleware instance.
        val accountAbnormalities2 = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        // mock accountManager doesn't have an account, but we expect it to have one since we
        // were authenticated before our "restart".
        accountAbnormalities2.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        assertCaughtException<AbnormalFxaEvent.MissingExpectedAccountAfterStartup>(crashReporter)
    }

    @Test
    fun `logout happy case`() = runBlocking {
        val crashReporter: CrashReporter = mockk()
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(testContext, crashReporter, this.coroutineContext)
        accountAbnormalities.accountManagerInitializedAsync(
            accountManager,
            CompletableDeferred(Unit).also { it.complete(Unit) }
        ).await()

        // We saw an auth event, then user requested a logout.
        accountAbnormalities.onAuthenticated(mockk(), mockk())
        accountAbnormalities.userRequestedLogout()
        verify { crashReporter wasNot Called }
    }

    private inline fun <reified T : AbnormalFxaEvent> assertCaughtException(crashReporter: CrashReporter) {
        verify {
            crashReporter.submitCaughtException(any<T>())
        }
    }
}
