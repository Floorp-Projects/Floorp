/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import kotlinx.coroutines.test.runTest
import mozilla.appservices.remotetabs.PendingCommand
import mozilla.appservices.remotetabs.RemoteCommand
import mozilla.appservices.remotetabs.RemoteCommandStore
import mozilla.appservices.remotetabs.TabsStore
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class RemoteTabsCommandQueueTest {
    private val storage: RemoteTabsStorage = mock()
    private val api: RemoteCommandStore = mock()
    private val closeTabsCommandSender: RemoteTabsCommandQueue.CommandSender<DeviceCommandOutgoing.CloseTab> = mock()
    private val commands = RemoteTabsCommandQueue(storage, closeTabsCommandSender)

    @Before
    fun setUp() {
        val storageApi: TabsStore = mock()
        whenever(storage.api).thenReturn(storageApi)
        whenever(storageApi.newRemoteCommandStore()).thenReturn(api)
    }

    @Test
    fun `GIVEN a queue WHEN a close tabs command is added THEN the queue should be updated`() = runTest {
        whenever(api.addRemoteCommand(any(), any())).thenReturn(true)

        val observer: DeviceCommandQueue.Observer = mock()
        commands.register(observer)

        commands.add("123", DeviceCommandOutgoing.CloseTab(listOf("http://example.com")))

        verify(api).addRemoteCommand(eq("123"), eq(RemoteCommand.CloseTab("http://example.com")))
        verify(observer).onAdded()
    }

    @Test
    fun `GIVEN a queue WHEN a close tabs command is removed THEN the queue should be updated`() = runTest {
        whenever(api.removeRemoteCommand(any(), any())).thenReturn(true)

        val observer: DeviceCommandQueue.Observer = mock()
        commands.register(observer)

        commands.remove("123", DeviceCommandOutgoing.CloseTab(listOf("http://example.com")))

        verify(api).removeRemoteCommand(eq("123"), eq(RemoteCommand.CloseTab("http://example.com")))
        verify(observer).onRemoved()
    }

    @Test
    fun `GIVEN a queue with unsent commands AND a sender that sends all commands WHEN the queue is flushed THEN the flush should succeed`() = runTest {
        whenever(api.getUnsentCommands()).thenReturn(
            listOf(
                PendingCommand(
                    deviceId = "123",
                    command = RemoteCommand.CloseTab("https://getfirefox.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
                PendingCommand(
                    deviceId = "1234",
                    command = RemoteCommand.CloseTab("https://getthunderbird.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
            ),
        )
        whenever(closeTabsCommandSender.send(any(), any())).thenReturn(RemoteTabsCommandQueue.SendResult.Ok)
        whenever(api.setPendingCommandSent(any())).thenReturn(true)

        assertEquals(DeviceCommandQueue.FlushResult.ok(), commands.flush())
        verify(closeTabsCommandSender, times(2)).send(any(), any())
        verify(api, times(2)).setPendingCommandSent(any())
    }

    @Test
    fun `GIVEN a queue with unsent commands AND a sender that returns 'no account' WHEN the queue is flushed THEN the flush should succeed`() = runTest {
        whenever(api.getUnsentCommands()).thenReturn(
            listOf(
                PendingCommand(
                    deviceId = "123",
                    command = RemoteCommand.CloseTab("https://getfirefox.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
            ),
        )
        whenever(closeTabsCommandSender.send(any(), any())).thenReturn(RemoteTabsCommandQueue.SendResult.NoAccount)

        assertEquals(DeviceCommandQueue.FlushResult.ok(), commands.flush())
        verify(closeTabsCommandSender).send(any(), any())
        verify(api, never()).setPendingCommandSent(any())
    }

    @Test
    fun `GIVEN a queue with unsent commands AND a sender that returns 'no device' WHEN the queue is flushed THEN the flush should succeed`() = runTest {
        whenever(api.getUnsentCommands()).thenReturn(
            listOf(
                PendingCommand(
                    deviceId = "123",
                    command = RemoteCommand.CloseTab("https://getfirefox.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
            ),
        )
        whenever(closeTabsCommandSender.send(any(), any())).thenReturn(RemoteTabsCommandQueue.SendResult.NoDevice)

        assertEquals(DeviceCommandQueue.FlushResult.ok(), commands.flush())
        verify(closeTabsCommandSender).send(any(), any())
        verify(api, never()).setPendingCommandSent(any())
    }

    @Test
    fun `GIVEN a queue with unsent commands AND a sender that returns an error WHEN the queue is flushed THEN the flush should be retried`() = runTest {
        whenever(api.getUnsentCommands()).thenReturn(
            listOf(
                PendingCommand(
                    deviceId = "123",
                    command = RemoteCommand.CloseTab("https://getfirefox.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
            ),
        )
        whenever(closeTabsCommandSender.send(any(), any())).thenReturn(RemoteTabsCommandQueue.SendResult.Error)

        assertEquals(DeviceCommandQueue.FlushResult.retry(), commands.flush())
        verify(closeTabsCommandSender).send(any(), any())
        verify(api, never()).setPendingCommandSent(any())
    }

    @Test
    fun `GIVEN a queue with unsent commands AND a sender that sends some commands WHEN the queue is flushed THEN the flush should be retried`() = runTest {
        whenever(api.getUnsentCommands()).thenReturn(
            listOf(
                PendingCommand(
                    deviceId = "123",
                    command = RemoteCommand.CloseTab("https://getfirefox.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
                PendingCommand(
                    deviceId = "1234",
                    command = RemoteCommand.CloseTab("https://getthunderbird.com"),
                    timeRequested = 0,
                    timeSent = null,
                ),
            ),
        )
        whenever(closeTabsCommandSender.send(eq("123"), any())).thenReturn(RemoteTabsCommandQueue.SendResult.Ok)
        whenever(closeTabsCommandSender.send(eq("1234"), any())).thenReturn(RemoteTabsCommandQueue.SendResult.Error)
        whenever(api.setPendingCommandSent(any())).thenReturn(true)

        assertEquals(DeviceCommandQueue.FlushResult.retry(), commands.flush())
        verify(closeTabsCommandSender, times(2)).send(any(), any())
        val pendingCommandCaptor = argumentCaptor<PendingCommand>()
        verify(api).setPendingCommandSent(pendingCommandCaptor.capture())
        assertEquals("123", pendingCommandCaptor.value.deviceId)
    }
}
