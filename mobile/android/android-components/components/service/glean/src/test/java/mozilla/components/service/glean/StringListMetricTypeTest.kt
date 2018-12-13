/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.StringListMetricType.Companion.MAX_STRING_LENGTH
import mozilla.components.service.glean.storages.StringListsStorageEngine
import mozilla.components.service.glean.storages.StringListsStorageEngineImplementation
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class StringListMetricTypeTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        StringListsStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        // Clear the stored "user" preferences between tests.
        ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(
                StringListsStorageEngine.javaClass.simpleName,
                Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        StringListsStorageEngine.clearAllStores()
    }

    @Test
    fun `The API must define the expected "default" storage`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )
        assertEquals(listOf("metrics"), stringListMetric.defaultStorageDestinations)
    }

    @Test
    fun `The API saves to its storage engine by first adding then setting`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        // Record two lists using add and set
        stringListMetric.add("value1")
        stringListMetric.add("value2")
        stringListMetric.add("value3")

        // Check that data was properly recorded.
        val snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals("value1", snapshot["telemetry.string_list_metric"]?.get(0))
        assertEquals("value2", snapshot["telemetry.string_list_metric"]?.get(1))
        assertEquals("value3", snapshot["telemetry.string_list_metric"]?.get(2))

        // Use set() to see that the first list is replaced by the new list
        stringListMetric.set(listOf("other1", "other2", "other3"))
        // Check that data was properly recorded.
        val snapshot2 = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.string_list_metric"))
        assertEquals("other1", snapshot2["telemetry.string_list_metric"]?.get(0))
        assertEquals("other2", snapshot2["telemetry.string_list_metric"]?.get(1))
        assertEquals("other3", snapshot2["telemetry.string_list_metric"]?.get(2))
    }

    @Test
    fun `The API saves to its storage engine by first setting then adding`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "string_list_metric",
                sendInPings = listOf("store1")
        )

        // Record two lists using set and add
        stringListMetric.set(listOf("value1", "value2", "value3"))

        // Check that data was properly recorded.
        val snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals("value1", snapshot["telemetry.string_list_metric"]?.get(0))
        assertEquals("value2", snapshot["telemetry.string_list_metric"]?.get(1))
        assertEquals("value3", snapshot["telemetry.string_list_metric"]?.get(2))

        // Use set() to see that the first list is replaced by the new list
        stringListMetric.add("added1")
        // Check that data was properly recorded.
        val snapshot2 = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.string_list_metric"))
        assertEquals("value1", snapshot2["telemetry.string_list_metric"]?.get(0))
        assertEquals("value2", snapshot2["telemetry.string_list_metric"]?.get(1))
        assertEquals("value3", snapshot2["telemetry.string_list_metric"]?.get(2))
        assertEquals("added1", snapshot2["telemetry.string_list_metric"]?.get(3))
    }

    @Test
    fun `The API truncates long string values`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1")
        )

        val longString = "a".repeat(MAX_STRING_LENGTH + 10)

        // Check that data was truncated via add() method.
        stringListMetric.add(longString)
        var snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_metric"))
        assertEquals(
            longString.take(MAX_STRING_LENGTH),
            snapshot["telemetry.string_metric"]?.get(0)
        )

        // Check that data was truncated via set() method.
        stringListMetric.set(listOf(longString))
        snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_metric"))
        assertEquals(
            longString.take(MAX_STRING_LENGTH),
            snapshot["telemetry.string_metric"]?.get(0)
        )
    }

    @Test
    fun `The API errors when attempting to add to a list beyond max`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        for (i in 1..StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE + 1) {
            stringListMetric.add("value$i")
        }

        // Check that list was truncated.
        val snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals(
            StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE,
            snapshot["telemetry.string_list_metric"]?.count()
        )
    }

    @Test
    fun `lists with no lifetime must not record data`() {
        // Define a string list metric which will be stored in "store1".
        // It's lifetime is set to Lifetime.Ping so it should not record anything.
        val stringListMetric = StringListMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the string list using set
        stringListMetric.set(listOf("value1", "value2", "value3"))
        // Check that nothing was recorded.
        val snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertNull("StringLists must not be recorded if they are disabled", snapshot)

        // Attempt to store the stringlist using add.
        stringListMetric.add("value4")
        // Check that nothing was recorded.
        val snapshot2 = StringListsStorageEngine.getSnapshot("store1", false)
        assertNull("StringLists must not be recorded if they are disabled", snapshot2)
    }

    @Test
    fun `disabled lists must not record data`() {
        // Define a string list metric which will be stored in "store1".
        // It's disabled so it should not record anything.
        val stringListMetric = StringListMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the string list using set.
        stringListMetric.set(listOf("value1", "value2", "value3"))
        // Check that nothing was recorded.
        val snapshot = StringListsStorageEngine.getSnapshot("store1", false)
        assertNull("StringLists must not be recorded if they are disabled", snapshot)

        // Attempt to store the string list using add.
        stringListMetric.add("value4")
        // Check that nothing was recorded.
        val snapshot2 = StringListsStorageEngine.getSnapshot("store1", false)
        assertNull("StringLists must not be recorded if they are disabled", snapshot2)
    }
}
