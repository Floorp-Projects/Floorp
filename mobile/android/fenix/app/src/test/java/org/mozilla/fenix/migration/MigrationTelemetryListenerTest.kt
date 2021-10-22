/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class MigrationTelemetryListenerTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @MockK(relaxed = true) private lateinit var metrics: MetricController
    @MockK(relaxed = true) private lateinit var logger: Logger
    private lateinit var store: MigrationStore
    private lateinit var listener: MigrationTelemetryListener

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        store = MigrationStore()
        listener = MigrationTelemetryListener(
            metrics = metrics,
            store = store,
            logger = logger
        )
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `progress state is logged`() = testDispatcher.runBlockingTest {
        listener.start()
        store.dispatch(MigrationAction.Started).joinBlocking()
        store.dispatch(MigrationAction.Completed).joinBlocking()
        store.dispatch(MigrationAction.Clear).joinBlocking()

        verifyOrder {
            logger.debug("Migration state: MIGRATING")
            logger.debug("Migration state: COMPLETED")
            logger.debug("Migration state: NONE")
        }
    }

    @Test
    fun `metrics are logged when migration is completed`() = testDispatcher.runBlockingTest {
        listener.start()
        store.dispatch(MigrationAction.Completed).joinBlocking()

        verify { metrics.track(Event.FennecToFenixMigrated) }
    }
}
