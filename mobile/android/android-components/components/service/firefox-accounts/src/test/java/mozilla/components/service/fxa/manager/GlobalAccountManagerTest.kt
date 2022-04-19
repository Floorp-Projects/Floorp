/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito

class GlobalAccountManagerTest {
    @ExperimentalCoroutinesApi
    @Test
    fun `GlobalAccountManager authError processing`() = runTest {
        val manager: FxaAccountManager = mock()
        GlobalAccountManager.setInstance(manager)

        val testClock: GlobalAccountManager.Clock = mock()
        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1L)

        GlobalAccountManager.authError("hello", testClock)
        Mockito.verify(manager).encounteredAuthError("hello", 1)

        // another error within a second, count goes up
        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1000L)
        GlobalAccountManager.authError("fxa oops", testClock)
        Mockito.verify(manager).encounteredAuthError("fxa oops", 2)

        // error five minutes later, count is reset
        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1000L * 60 * 5)
        GlobalAccountManager.authError("fxa oops 2", testClock)
        Mockito.verify(manager).encounteredAuthError("fxa oops 2", 1)

        // the count is ramped up if auth errors become frequent again
        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1000L * 60 * 5 + 1000L)
        GlobalAccountManager.authError("fxa oops 2", testClock)
        Mockito.verify(manager).encounteredAuthError("fxa oops 2", 2)

        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1000L * 60 * 5 + 2000L)
        GlobalAccountManager.authError("fxa oops 2", testClock)
        Mockito.verify(manager).encounteredAuthError("fxa oops 2", 3)

        // ... and is reset again as errors slow down.
        Mockito.`when`(testClock.getTimeCheckPoint()).thenReturn(1000L * 60 * 5 + 1000L * 60 * 15)
        GlobalAccountManager.authError("profile fetch", testClock)
        Mockito.verify(manager).encounteredAuthError("profile fetch", 1)
    }
}
