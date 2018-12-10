/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.storages.UuidsStorageEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.UUID

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class UuidMetricTypeTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        Glean.initialized = true
        UuidsStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        // Clear the stored "user" preferences between tests.
        ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(UuidsStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        UuidsStorageEngine.clearAllStores()
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in "store1"
        val uuidMetric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuid_metric",
            sendInPings = listOf("store1")
        )

        // Record two uuids of the same type, with a little delay.
        val uuid = uuidMetric.generateAndSet()

        // Check that data was properly recorded.
        val snapshot = UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.uuid_metric"))
        assertEquals(uuid, snapshot.get("telemetry.uuid_metric"))

        val uuid2 = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")
        uuidMetric.set(uuid2)

        // Check that data was properly recorded.
        val snapshot2 = UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.uuid_metric"))
        assertEquals(uuid2, snapshot2.get("telemetry.uuid_metric"))
    }

    @Test
    fun `uuids with no lifetime must not record data`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in
        // "store1". It's disabled so it should not record anything.
        val uuidMetric = UuidMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "uuidMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the uuid.
        uuidMetric.generateAndSet()
        // Check that nothing was recorded.
        val snapshot = UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Uuids must not be recorded if they are disabled", snapshot)
    }

    @Test
    fun `disabled uuids must not record data`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val uuidMetric = UuidMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuidMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the uuid.
        uuidMetric.generateAndSet()
        // Check that nothing was recorded.
        val snapshot = UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Uuids must not be recorded if they are disabled", snapshot)
    }
}
