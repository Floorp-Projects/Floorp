/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.verify

class CloseTabsUseCasesTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val commands: DeviceCommandQueue<DeviceCommandQueue.Type.RemoteTabs> = mock()
    private val useCases = CloseTabsUseCases(commands)

    @Test
    fun `WHEN a tab is closed on another device THEN a command to close the tab is added to the queue`() = runTestOnMain {
        whenever(commands.add(any(), any())).thenReturn(Unit)

        useCases.close("123", "http://example.com")

        verify(commands).add(eq("123"), any())
    }
}
