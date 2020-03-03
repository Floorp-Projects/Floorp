/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Intent
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MigrationIntentProcessorTest {
    @Test
    fun `process updates intent`() = runBlockingTest {
        val store = MigrationStore()
        val processor = MigrationIntentProcessor(store)
        val intent: Intent = mock()

        assertFalse(processor.process(intent))

        store.dispatch(MigrationAction.Completed).joinBlocking()
        assertFalse(processor.process(intent))

        store.dispatch(MigrationAction.Started).joinBlocking()
        assertTrue(processor.process(intent))
    }
}