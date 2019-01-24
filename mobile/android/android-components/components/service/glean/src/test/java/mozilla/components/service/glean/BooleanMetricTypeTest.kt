/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.storages.BooleansStorageEngine
import org.junit.Assert.assertNull
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class BooleanMetricTypeTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        Glean.initialized = true
        BooleansStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        // Clear the stored "user" preferences between tests.
        ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(BooleansStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        BooleansStorageEngine.clearAllStores()
    }

    @Test
    fun `The API must define the expected "default" storage`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )
        assertEquals(listOf("metrics"), booleanMetric.defaultStorageDestinations)
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )

        // Record two booleans of the same type, with a little delay.
        booleanMetric.set(true)

        // Check that data was properly recorded.
        val snapshot = BooleansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.boolean_metric"))
        assertEquals(true, snapshot.get("telemetry.boolean_metric"))

        booleanMetric.set(false)
        // Check that data was properly recorded.
        val snapshot2 = BooleansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.boolean_metric"))
        assertEquals(false, snapshot2.get("telemetry.boolean_metric"))
    }

    @Test
    fun `disabled booleans must not record data`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val booleanMetric = BooleanMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "booleanMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the boolean.
        booleanMetric.set(true)
        // Check that nothing was recorded.
        val snapshot = BooleansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Booleans must not be recorded if they are disabled", snapshot)
    }
}
