/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.perf.TestStrictModeManager
import org.mozilla.fenix.perf.StrictModeManager

@RunWith(FenixRobolectricTestRunner::class)
class AccountAbnormalitiesTest {
    @Test
    fun `account manager must be configured`() {
        val crashReporter: CrashReporter = mockk()

        // no account present
        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )

        try {
            accountAbnormalities.userRequestedLogout()
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("userRequestedLogout before account manager was configured", e.message)
        }

        // This doesn't throw, see method for details.
        accountAbnormalities.onAuthenticated(mockk(), mockk())

        try {
            accountAbnormalities.onLoggedOut()
            fail()
        } catch (e: IllegalStateException) {
            assertEquals("onLoggedOut before account manager was configured", e.message)
        }

        verify { crashReporter wasNot Called }
    }

    @Test
    fun `LogoutWithoutAuth detected`() = runTest {
        val crashReporter: CrashReporter = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        accountAbnormalities.onReady(mockk(relaxed = true))

        // Logout action must be preceded by auth.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException<AbnormalFxaEvent.LogoutWithoutAuth>(crashReporter)
    }

    @Test
    fun `OverlappingFxaLogoutRequest detected`() = runTest {
        val crashReporter: CrashReporter = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        accountAbnormalities.onReady(mockk(relaxed = true))

        accountAbnormalities.onAuthenticated(mockk(), mockk())
        // So far, so good. A regular logout request while being authenticated.
        accountAbnormalities.userRequestedLogout()
        verify { crashReporter wasNot Called }

        // We never saw a logout callback after previous logout request, so this is an overlapping request.
        accountAbnormalities.userRequestedLogout()
        assertCaughtException<AbnormalFxaEvent.OverlappingFxaLogoutRequest>(crashReporter)
    }

    @Test
    fun `callback logout abnormalities detected`() = runTest {
        val crashReporter: CrashReporter = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        accountAbnormalities.onReady(mockk(relaxed = true))

        // User didn't request this logout.
        accountAbnormalities.onLoggedOut()
        assertCaughtException<AbnormalFxaEvent.UnexpectedFxaLogout>(crashReporter)
    }

    @Test
    fun `login happy case + disappearing account detected`() = runTest {
        val crashReporter: CrashReporter = mockk(relaxed = true)
        val accountManager: FxaAccountManager = mockk(relaxed = true)

        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        accountAbnormalities.onReady(null)

        accountAbnormalities.onAuthenticated(mockk(), mockk())
        verify { crashReporter wasNot Called }
        every { accountManager.authenticatedAccount() } returns null

        // Pretend we restart, and instantiate a new middleware instance.
        val accountAbnormalities2 = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        // mock accountManager doesn't have an account, but we expect it to have one since we
        // were authenticated before our "restart".
        accountAbnormalities2.onReady(null)

        assertCaughtException<AbnormalFxaEvent.MissingExpectedAccountAfterStartup>(crashReporter)
    }

    @Test
    fun `logout happy case`() = runTest {
        val crashReporter: CrashReporter = mockk()

        val accountAbnormalities = AccountAbnormalities(
            testContext,
            crashReporter,
            TestStrictModeManager() as StrictModeManager,
        )
        accountAbnormalities.onReady(mockk(relaxed = true))

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
