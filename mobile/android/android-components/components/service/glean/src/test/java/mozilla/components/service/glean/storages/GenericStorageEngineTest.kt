/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.Lifetime
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GenericStorageEngineTest {
    private data class GenericMetricType(
        override val disabled: Boolean,
        override val category: String,
        override val lifetime: Lifetime,
        override val name: String,
        override val sendInPings: List<String>
    ) : CommonMetricData

    @Test
    fun `metrics with 'user' lifetime must not be cleared when snapshotting`() {
        val dataUserLifetime = 37
        val dataPingLifetime = 3

        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

        val metric1 = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "userLifetimeData",
            sendInPings = listOf("store1")
        )

        // Record a value with User lifetime
        storageEngine.record(
            metric1,
            value = dataUserLifetime
        )

        val metric2 = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "pingLifetimeData",
            sendInPings = listOf("store1")
        )

        // Record a value with Ping lifetime
        storageEngine.record(
            metric2,
            value = dataPingLifetime
        )

        // Take a snapshot and clear the store: this snapshot must contain the data for
        // both metrics.
        val firstSnapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(2, firstSnapshot!!.size)
        assertEquals(dataUserLifetime, firstSnapshot["telemetry.userLifetimeData"])
        assertEquals(dataPingLifetime, firstSnapshot["telemetry.pingLifetimeData"])

        // Take a new snapshot. The ping lifetime data should have been cleared and not be
        // available anymore, data with 'user' lifetime must still be around.
        val secondSnapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(1, secondSnapshot!!.size)
        assertEquals(dataUserLifetime, secondSnapshot["telemetry.userLifetimeData"])
        assertFalse(secondSnapshot.contains("telemetry.pingLifetimeData"))
    }

    @Test
    fun `metrics with empty 'category' must be properly recorded`() {
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

        val metric = GenericMetricType(
            disabled = false,
            category = "",
            lifetime = Lifetime.Ping,
            name = "noCategoryName",
            sendInPings = listOf("store1")
        )

        // Record a value with User lifetime
        storageEngine.record(
            metric,
            value = 37
        )

        // Take a snapshot and clear the store: this snapshot must contain the data for
        // both metrics.
        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(37, snapshot["noCategoryName"])
    }

    @Test
    fun `metric with 'user' lifetime must be correctly unpersisted when recording 'user' values`() {
        // Make up the test data that we pretend to be unserialized.
        val persistedSample = mapOf(
            "store1#telemetry.value1" to 1,
            "store1#telemetry.value2" to 2,
            "store2#telemetry.value1" to 1
        )

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { persistedSample }
        `when`(context.getSharedPreferences(
            eq(MockGenericStorageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)

        // Instantiate our mock engine and check that it correctly unpersists the
        // data and makes it available in the snapshot.
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        val metric = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "pingLifetimeData",
            sendInPings = listOf("store1")
        )

        // Record a value with Ping lifetime
        storageEngine.record(
            metric,
            value = 37
        )

        verify(sharedPreferences, times(1)).all
    }

    @Test
    fun `unpersisting broken 'user' lifetime data should not break the API`() {
        val brokenSample = mapOf(
            "store1#telemetry.value1" to "test",
            "store1#telemetry.value2" to false,
            "store1#telemetry.value1" to null
        )

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { brokenSample }
        `when`(context.getSharedPreferences(
            eq(MockGenericStorageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        // Instantiate our mock engine and check that it correctly unpersists the
        // data and makes it available in the snapshot.
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        val metric = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "pingLifetimeData",
            sendInPings = listOf("store1")
        )

        // Record a value with Ping lifetime
        storageEngine.record(
            metric,
            value = 37
        )

        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(37, snapshot["telemetry.pingLifetimeData"])
    }

    @Test
    fun `unpersisting metrics must skip data with missing storage name`() {
        val brokenSample = mapOf(
            "store_name#telemetry.value1" to 11,
            "telemetry.value2" to 7,
            "#telemetry.value3" to 15
        )

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { brokenSample }
        `when`(context.getSharedPreferences(
            eq(MockGenericStorageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        // Instantiate our mock engine and check that it correctly unpersists the
        // data and makes it available in the snapshot.
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        val snapshot = storageEngine.getSnapshot(storeName = "store_name", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(11, snapshot["telemetry.value1"])
    }

    @Test
    fun `unpersisting metrics must not fail if SharedPreferences throws`() {
        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenThrow(NullPointerException())
        `when`(context.getSharedPreferences(
            eq(MockGenericStorageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        // Instantiate our mock engine and check that it correctly unpersists the
        // data and makes it available in the snapshot.
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        // Make sure we attempt to load data to trigger the exception.
        storageEngine.getSnapshot(storeName = "store_name", clearStore = true)
        // The next call verifies that we're called twice: one directly by the snapshot, the other
        // indirectly by the internal lazy loading function.
        verify(sharedPreferences, times(2)).all
    }

    @Test
    fun `metrics with 'user' lifetime must be correctly unpersisted before taking snapshots`() {
        // Make up the test data that we pretend to be unserialized.
        val persistedSample = mapOf(
            "store1#telemetry.value1" to 1,
            "store1#telemetry.value2" to 2,
            "store2#telemetry.value1" to 1
        )

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { persistedSample }
        `when`(context.getSharedPreferences(
            eq(MockGenericStorageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${MockGenericStorageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        // Instantiate our mock engine and check that it correctly unpersists the
        // data and makes it available in the snapshot.
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        val store1Snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(2, store1Snapshot!!.size)
        assertEquals(1, store1Snapshot["telemetry.value1"])
        assertEquals(2, store1Snapshot["telemetry.value2"])

        val store2Snapshot = storageEngine.getSnapshot(storeName = "store2", clearStore = true)
        assertEquals(1, store2Snapshot!!.size)
        assertEquals(1, store2Snapshot["telemetry.value1"])
    }

    @Test
    fun `snapshotting must only clear 'ping' lifetime`() {
        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

        val stores = listOf("store1", "store2")

        val metric1 = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "userLifetimeData",
            sendInPings = stores
        )

        // Record a value with User lifetime
        storageEngine.record(
            metric1,
            value = 11
        )

        val metric2 = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "applicationLifetimeData",
            sendInPings = stores
        )

        // Record a value with Application lifetime
        storageEngine.record(
            metric2,
            value = 7
        )

        val metric3 = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "pingLifetimeData",
            sendInPings = stores
        )

        // Record a value with Ping lifetime
        storageEngine.record(
            metric3,
            value = 2015
        )

        for (store in stores) {
            // Get a first snapshot: it will clear the "ping" lifetime for the
            // requested store.
            val snapshot1 = storageEngine.getSnapshot(storeName = store, clearStore = true)
            assertEquals(3, snapshot1!!.size)
            assertEquals(11, snapshot1["telemetry.userLifetimeData"])
            assertEquals(7, snapshot1["telemetry.applicationLifetimeData"])
            assertEquals(2015, snapshot1["telemetry.pingLifetimeData"])

            // A new snapshot should not contain data with "ping" lifetime, since it was
            // previously cleared.
            val snapshot2 = storageEngine.getSnapshot(storeName = store, clearStore = true)
            assertEquals(2, snapshot2!!.size)
            assertEquals(11, snapshot2["telemetry.userLifetimeData"])
            assertEquals(7, snapshot2["telemetry.applicationLifetimeData"])
        }
    }

    @Test
    fun `metrics with 'application' lifetime must be cleared when the application is closed`() {
        // We use block scopes to simulate restarting the application. We use the same
        // context otherwise the test environment will use a different underlying file
        // for the SharedPreferences.
        run {
            val storageEngine = MockGenericStorageEngine()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric1 = GenericMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "userLifetimeData",
                sendInPings = listOf("store1")
            )

            // Record a value with User lifetime
            storageEngine.record(
                metric1,
                value = 37
            )

            val metric2 = GenericMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "applicationLifetimeData",
                sendInPings = listOf("store1")
            )

            // Record a value with Application lifetime
            storageEngine.record(
                metric2,
                value = 85
            )

            // Make sure the data was recorded without clearing the storage.
            val snapshot = storageEngine.getSnapshot("store1", false)
            assertEquals(2, snapshot!!.size)
            assertEquals(37, snapshot["telemetry.userLifetimeData"])
            assertEquals(85, snapshot["telemetry.applicationLifetimeData"])
        }

        // Re-instantiate the engine: application lifetime probes should have been cleared.
        run {
            val storageEngine = MockGenericStorageEngine()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val snapshot = storageEngine.getSnapshot("store1", true)
            assertEquals(1, snapshot!!.size)
            assertEquals(37, snapshot["telemetry.userLifetimeData"])
        }
    }

    @Test
    fun `metrics with 'ping' lifetime must be cleared when the ping is scheduled`() {
        // We use block scopes to simulate restarting the application. We use the same
        // context otherwise the test environment will use a different underlying file
        // for the SharedPreferences.
        run {
            val storageEngine = MockGenericStorageEngine()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric1 = GenericMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "userLifetimeData",
                sendInPings = listOf("store1")
            )

            // Record a value with User lifetime
            storageEngine.record(
                metric1,
                value = 37
            )

            val metric2 = GenericMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Ping,
                name = "pingLifetimeData",
                sendInPings = listOf("store1", "store2")
            )

            // Record a value with Application lifetime
            storageEngine.record(
                metric2,
                value = 85
            )

            // Make sure the data was recorded without clearing the storage.
            val snapshot = storageEngine.getSnapshot("store1", false)
            assertEquals(2, snapshot!!.size)
            assertEquals(37, snapshot["telemetry.userLifetimeData"])
            assertEquals(85, snapshot["telemetry.pingLifetimeData"])

            // Verify data was recorded in the second ping "store2"
            val snapshot2 = storageEngine.getSnapshot("store2", false)
            assertEquals(1, snapshot2!!.size)
            assertEquals(85, snapshot2["telemetry.pingLifetimeData"])
        }

        // Re-instantiate the engine: ping lifetime metrics should be persisted.
        run {
            val storageEngine = MockGenericStorageEngine()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val snapshot = storageEngine.getSnapshot("store1", true)
            assertEquals(2, snapshot!!.size)
            assertEquals(37, snapshot["telemetry.userLifetimeData"])
            // Ensure the ping lifetime was persisted
            assertEquals(85, snapshot["telemetry.pingLifetimeData"])

            // Check that "store2" was persisted
            val snapshot2 = storageEngine.getSnapshot("store2", false)
            assertEquals(1, snapshot2!!.size)
            // Ensure the ping lifetime was persisted
            assertEquals(85, snapshot2["telemetry.pingLifetimeData"])
        }

        // Now that the store was cleared, ping lifetime should have nothing persisted
        run {
            val storageEngine = MockGenericStorageEngine()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val snapshot = storageEngine.getSnapshot("store1", true)
            assertEquals(1, snapshot!!.size)
            assertEquals(37, snapshot["telemetry.userLifetimeData"])
            // Ensure the ping lifetime was not persisted
            assertNull(snapshot["telemetry.pingLifetimeData"])

            // Check that "store2" was persisted, passing true to go ahead and clear store2 out
            val snapshot2 = storageEngine.getSnapshot("store2", true)
            assertEquals(1, snapshot2!!.size)
            // Ensure the ping lifetime was persisted
            assertEquals(85, snapshot2["telemetry.pingLifetimeData"])
        }
    }
}
