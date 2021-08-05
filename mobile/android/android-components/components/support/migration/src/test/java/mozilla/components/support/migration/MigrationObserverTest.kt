/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class MigrationObserverTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `listener is invoked on state observation`() {
        val store = MigrationStore()
        val listener: MigrationStateListener = mock()
        val observer = MigrationObserver(store, listener)

        verifyNoInteractions(listener)

        observer.start()

        verify(listener).onMigrationCompleted(any())

        store.dispatch(MigrationAction.Started).joinBlocking()
        testDispatcher.advanceUntilIdle()

        verify(listener).onMigrationStateChanged(eq(MigrationProgress.MIGRATING), any())
    }

    @Test
    fun `listener is not invoked when stopped`() {
        val store = MigrationStore()
        val listener: MigrationStateListener = mock()
        val observer = MigrationObserver(store, listener)

        verifyNoInteractions(listener)

        observer.start()
        observer.stop()

        store.dispatch(MigrationAction.Started).joinBlocking()
        testDispatcher.advanceUntilIdle()

        verify(listener, never()).onMigrationStateChanged(any(), any())
    }
}
