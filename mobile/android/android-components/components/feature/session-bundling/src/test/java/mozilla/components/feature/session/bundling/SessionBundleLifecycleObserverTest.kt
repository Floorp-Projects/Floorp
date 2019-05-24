/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.feature.session.bundling

import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class SessionBundleLifecycleObserverTest {
    @Test
    fun `New bundle is started and sessions are removed from SessionManager`() {
        runBlocking {
            val sessionManager: SessionManager = mock()

            val storage: SessionBundleStorage = mock()
            doReturn(Pair(0L, TimeUnit.SECONDS)).`when`(storage).bundleLifetime

            val observer = SessionBundleLifecycleObserver(storage, sessionManager)
            observer.mainDispatcher = Executors.newSingleThreadScheduledExecutor().asCoroutineDispatcher()

            observer.onStop()

            assertNotNull(observer.backgroundJob)
            observer.backgroundJob!!.join()

            verify(storage).new()
            verify(sessionManager).removeSessions()
        }
    }

    @Test
    fun `Nothing is done if app resumes before lifetime expires`() {
        runBlocking {
            val sessionManager: SessionManager = mock()

            val storage: SessionBundleStorage = mock()
            doReturn(Pair(10L, TimeUnit.SECONDS)).`when`(storage).bundleLifetime

            val observer = SessionBundleLifecycleObserver(storage, sessionManager)
            observer.mainDispatcher = Executors.newSingleThreadScheduledExecutor().asCoroutineDispatcher()

            observer.onStop()

            delay(1000)

            observer.onStart()

            assertNotNull(observer.backgroundJob)
            observer.backgroundJob!!.join()

            verify(storage, never()).new()
            verify(sessionManager, never()).removeSessions()
        }
    }
}
