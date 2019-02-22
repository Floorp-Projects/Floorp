/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit

class GeneralSyncManagerTest {
    class TestSyncManager(
        private val dispatcher: SyncDispatcher,
        val dispatcherValidator: (stores: Set<String>, account: OAuthAccount) -> Unit
    ) : GeneralSyncManager() {
        override fun createDispatcher(stores: Set<String>, account: OAuthAccount): SyncDispatcher {
            dispatcherValidator(stores, account)
            return dispatcher
        }

        override fun dispatcherUpdated(dispatcher: SyncDispatcher) {
        }
    }

    @Test
    fun `modifying stores should not reset dispatcher if there's no account`() {
        val manager = TestSyncManager(mock()) { _, _ ->
            fail()
        }

        manager.addStore("history")
        manager.addStore("logins")
        manager.removeStore("logins")
    }

    @Test
    fun `dispatcher resets should happen once account is set`() {
        val dispatcher = mock<SyncDispatcher>()
        var storez: Set<String>? = null
        var akaunt: OAuthAccount? = null

        val manager = TestSyncManager(dispatcher) { stores, account ->
            storez = stores
            akaunt = account
        }

        val mockAccount: OAuthAccount = mock()

        verify(dispatcher, never()).syncNow()
//        verify(dispatcher, never()).startPeriodicSync(any(), any())

        manager.addStore("history")
        manager.authenticated(mockAccount)

        verify(dispatcher).syncNow()
//        verify(dispatcher).startPeriodicSync(any(), any())

        assertEquals(1, storez!!.size)
        assertTrue(storez!!.contains("history"))
        assertEquals(mockAccount, akaunt)

        // Adding the same store multiple times is a no-op.
        manager.addStore("history")
        assertEquals(1, storez!!.size)
        assertTrue(storez!!.contains("history"))
        assertEquals(mockAccount, akaunt)

        verify(dispatcher, times(2)).syncNow()
//        verify(dispatcher, times(2)).startPeriodicSync(any(), any())

        manager.addStore("logins")
        assertEquals(2, storez!!.size)
        assertTrue(storez!!.contains("history"))
        assertTrue(storez!!.contains("logins"))
        assertEquals(mockAccount, akaunt)

        verify(dispatcher, times(3)).syncNow()
//        verify(dispatcher, times(3)).startPeriodicSync(any(), any())

        manager.removeStore("history")
        assertEquals(1, storez!!.size)
        assertTrue(storez!!.contains("logins"))
        assertEquals(mockAccount, akaunt)

        verify(dispatcher, times(4)).syncNow()
//        verify(dispatcher, times(4)).startPeriodicSync(any(), any())
    }

    @Test(expected = KotlinNullPointerException::class)
    fun `logging out before being authenticated is a mistake`() {
        val manager = TestSyncManager(mock()) { _, _ ->
            fail()
        }
        manager.loggedOut()
    }

    @Test
    fun `logging out should reset an account and clean up the dispatcher`() {
        val dispatcher: SyncDispatcher = mock()
        val account: OAuthAccount = mock()
        val manager = TestSyncManager(dispatcher) { _, _ -> }

        manager.authenticated(account)
        verify(dispatcher, never()).stopPeriodicSync()
        verify(dispatcher, never()).close()

        manager.loggedOut()

        verify(dispatcher).stopPeriodicSync()
        verify(dispatcher).close()
    }

    @Test(expected = KotlinNullPointerException::class)
    fun `logging out twice is a mistake`() {
        val account: OAuthAccount = mock()
        val manager = TestSyncManager(mock()) { _, _ -> }

        manager.authenticated(account)
        manager.loggedOut()
        manager.loggedOut()
    }

    @Test
    fun `logging in after logging out should be possible with the same account`() {
        val mockAccount: OAuthAccount = mock()
        val dispatcher: SyncDispatcher = mock()

        var akaunt: OAuthAccount? = null
        var storez: Set<String>? = null

        val manager = TestSyncManager(dispatcher) { stores, account ->
            storez = stores
            akaunt = account
        }

        manager.authenticated(mockAccount)
        verify(dispatcher).syncNow()
        assertEquals(mockAccount, akaunt)
        assertTrue(storez!!.isEmpty())

        manager.loggedOut()

        manager.authenticated(mockAccount)

        verify(dispatcher, times(2)).syncNow()
        assertEquals(mockAccount, akaunt)
        assertTrue(storez!!.isEmpty())
    }

    @Test
    fun `logging in after logging out should be possible with different accounts`() {
        val mockAccount1: OAuthAccount = mock()
        val dispatcher: SyncDispatcher = mock()

        var akaunt: OAuthAccount? = null
        var storez: Set<String>? = null

        val manager = TestSyncManager(dispatcher) { stores, account ->
            storez = stores
            akaunt = account
        }

        manager.authenticated(mockAccount1)
        verify(dispatcher).syncNow()
        assertEquals(mockAccount1, akaunt)
        assertTrue(storez!!.isEmpty())

        manager.loggedOut()

        val mockAccount2: OAuthAccount = mock()

        manager.authenticated(mockAccount2)

        verify(dispatcher, times(2)).syncNow()
        assertEquals(mockAccount2, akaunt)
        assertTrue(storez!!.isEmpty())
    }

    @Test
    fun `manager should correctly relay current sync status`() {
        val dispatcher: SyncDispatcher = mock()
        val manager = TestSyncManager(dispatcher) { _, _ -> }

        `when`(dispatcher.isSyncActive()).thenReturn(false)

        assertFalse(manager.isSyncRunning())

        manager.authenticated(mock())
        assertFalse(manager.isSyncRunning())

        `when`(dispatcher.isSyncActive()).thenReturn(true)
        assertTrue(manager.isSyncRunning())
    }

    @Test
    fun `manager should notify its observers of sync state`() {
        val dispatcher: SyncDispatcher = object : SyncDispatcher, Observable<SyncStatusObserver> by ObserverRegistry() {
            override fun isSyncActive(): Boolean { return false }
            override fun syncNow(startup: Boolean) {}
            override fun startPeriodicSync(unit: TimeUnit, period: Long) {}
            override fun stopPeriodicSync() {}
            override fun workersStateChanged(isRunning: Boolean) {}
            override fun close() {}
        }

        val manager = TestSyncManager(dispatcher) { _, _ -> }

        manager.authenticated(mock())

        val statusObserver: SyncStatusObserver = mock()
        manager.register(statusObserver)

        verify(statusObserver, never()).onError(any())
        verify(statusObserver, never()).onIdle()
        verify(statusObserver, never()).onStarted()

        dispatcher.notifyObservers { onStarted() }

        verify(statusObserver).onStarted()

        dispatcher.notifyObservers { onIdle() }

        verify(statusObserver).onIdle()

        dispatcher.notifyObservers { onError(null) }

        verify(statusObserver).onError(null)

        val exception = IllegalStateException()

        dispatcher.notifyObservers { onError(exception) }

        verify(statusObserver).onError(exception)
    }

    @Test(expected = KotlinNullPointerException::class)
    fun `calling syncNow on startup before authenticating is a mistake`() {
        val manager = TestSyncManager(mock()) { _, _ -> }
        manager.syncNow(true)
    }

    @Test(expected = KotlinNullPointerException::class)
    fun `calling syncNow not on startup before authenticating is a mistake`() {
        val manager = TestSyncManager(mock()) { _, _ -> }
        manager.syncNow(false)
    }

    @Test
    fun `manager should relay syncNow calls`() {
        val dispatcher: SyncDispatcher = mock()
        val manager = TestSyncManager(dispatcher) { _, _ -> }

        manager.authenticated(mock())

        verify(dispatcher, never()).syncNow(true)
        // Authentication should cause a syncNow call.
        verify(dispatcher).syncNow(false)

        manager.syncNow(true)
        verify(dispatcher).syncNow(true)

        manager.syncNow(false)
        verify(dispatcher, times(2)).syncNow(false)
    }

    @Test
    fun `manager should relay syncNow calls by default as not on startup`() {
        val dispatcher: SyncDispatcher = mock()
        val manager = TestSyncManager(dispatcher) { _, _ -> }

        manager.authenticated(mock())

        verify(dispatcher).syncNow(false)

        manager.syncNow()
        verify(dispatcher, times(2)).syncNow(false)
    }
}