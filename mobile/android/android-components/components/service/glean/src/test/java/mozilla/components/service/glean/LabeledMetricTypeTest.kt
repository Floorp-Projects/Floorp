/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.storages.BooleansStorageEngine
import mozilla.components.service.glean.storages.CountersStorageEngine
import mozilla.components.service.glean.storages.MockGenericStorageEngine
import mozilla.components.service.glean.storages.StringListsStorageEngine
import mozilla.components.service.glean.storages.StringsStorageEngine
import mozilla.components.service.glean.storages.TimespansStorageEngine
import mozilla.components.service.glean.storages.UuidsStorageEngine
import mozilla.components.service.glean.error.ErrorRecording
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import java.util.UUID
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class LabeledMetricTypeTest {
    private data class GenericMetricType(
        override val disabled: Boolean,
        override val category: String,
        override val lifetime: Lifetime,
        override val name: String,
        override val sendInPings: List<String>
    ) : CommonMetricData {
        override val defaultStorageDestinations: List<String> = listOf("metrics")
    }

    @Before
    fun setup() {
        resetGlean()
    }

    @After
    fun resetGlobalState() {
        Glean.setUploadEnabled(true)
    }

    @Test
    fun `test labeled counter type`() {
        CountersStorageEngine.clearAllStores()

        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default")
        )

        val labeledCounterMetric = LabeledMetricType<CounterMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default"),
            subMetric = counterMetric
        )

        CountersStorageEngine.record(labeledCounterMetric["label1"], 1)
        CountersStorageEngine.record(labeledCounterMetric["label2"], 2)

        // Record a regular non-labeled counter. This isn't normally
        // possible with the generated code because the subMetric is private,
        // but it's useful to test here that it works.
        CountersStorageEngine.record(counterMetric, 3)

        val snapshot = CountersStorageEngine.getSnapshot(storeName = "metrics", clearStore = false)

        assertEquals(3, snapshot!!.size)
        assertEquals(1, snapshot.get("telemetry.labeled_counter_metric/label1"))
        assertEquals(2, snapshot.get("telemetry.labeled_counter_metric/label2"))
        assertEquals(3, snapshot.get("telemetry.labeled_counter_metric"))

        val json = collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
        // Do the same checks again on the JSON structure
        assertEquals(
            1,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")!!
                .get("label1")
        )
        assertEquals(
            2,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")!!
                .get("label2")
        )
        assertEquals(
            3,
            json.getJSONObject("counter")!!
                .get("telemetry.labeled_counter_metric")
        )
    }

    @Test
    fun `test __other__ label with predefined labels`() {
        CountersStorageEngine.clearAllStores()

        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default")
        )

        val labeledCounterMetric = LabeledMetricType<CounterMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default"),
            subMetric = counterMetric,
            labels = setOf("foo", "bar", "baz")
        )

        CountersStorageEngine.record(labeledCounterMetric["foo"], 1)
        CountersStorageEngine.record(labeledCounterMetric["foo"], 1)
        CountersStorageEngine.record(labeledCounterMetric["bar"], 1)
        CountersStorageEngine.record(labeledCounterMetric["not_there"], 1)
        CountersStorageEngine.record(labeledCounterMetric["also_not_there"], 1)
        CountersStorageEngine.record(labeledCounterMetric["not_me"], 1)

        val snapshot = CountersStorageEngine.getSnapshot(storeName = "metrics", clearStore = false)

        assertEquals(3, snapshot!!.size)
        assertEquals(2, snapshot.get("telemetry.labeled_counter_metric/foo"))
        assertEquals(1, snapshot.get("telemetry.labeled_counter_metric/bar"))
        assertNull(snapshot.get("telemetry.labeled_counter_metric/baz"))
        assertEquals(3, snapshot.get("telemetry.labeled_counter_metric/__other__"))

        val json = collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
        // Do the same checks again on the JSON structure
        assertEquals(
            2,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")
                .get("foo")
        )
        assertEquals(
            1,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")
                .get("bar")
        )
        assertEquals(
            3,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")
                .get("__other__")
        )
    }

    @Test
    fun `test __other__ label without predefined labels`() {
        CountersStorageEngine.clearAllStores()

        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default")
        )

        val labeledCounterMetric = LabeledMetricType<CounterMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default"),
            subMetric = counterMetric
        )

        for (i in 0..20) {
            CountersStorageEngine.record(labeledCounterMetric["label_$i"], 1)
        }
        // Go back and record in one of the real labels again
        CountersStorageEngine.record(labeledCounterMetric["label_0"], 1)

        val snapshot = CountersStorageEngine.getSnapshot(storeName = "metrics", clearStore = false)

        assertEquals(17, snapshot!!.size)
        assertEquals(2, snapshot.get("telemetry.labeled_counter_metric/label_0"))
        for (i in 1..15) {
            assertEquals(1, snapshot.get("telemetry.labeled_counter_metric/label_$i"))
        }
        assertEquals(5, snapshot.get("telemetry.labeled_counter_metric/__other__"))

        val json = collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
        // Do the same checks again on the JSON structure
        assertEquals(
            2,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")!!
                .get("label_0")
        )
        for (i in 1..15) {
            assertEquals(
                1,
                json.getJSONObject("labeled_counter")!!
                    .getJSONObject("telemetry.labeled_counter_metric")!!
                    .get("label_$i")
            )
        }
        assertEquals(
            5,
            json.getJSONObject("labeled_counter")!!
                .getJSONObject("telemetry.labeled_counter_metric")!!
                .get("__other__")
        )
    }

    @Test
    fun `Ensure invalid labels go to __other__`() {
        CountersStorageEngine.clearAllStores()

        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default")
        )

        val labeledCounterMetric = LabeledMetricType<CounterMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_counter_metric",
            sendInPings = listOf("default"),
            subMetric = counterMetric
        )

        CountersStorageEngine.record(labeledCounterMetric["notSnakeCase"], 1)
        CountersStorageEngine.record(labeledCounterMetric[""], 1)
        CountersStorageEngine.record(labeledCounterMetric["with/slash"], 1)
        CountersStorageEngine.record(
            labeledCounterMetric["this_string_has_more_than_thirty_characters"],
            1
        )

        assertEquals(
            4,
            ErrorRecording.testGetNumRecordedErrors(
                labeledCounterMetric,
                ErrorRecording.ErrorType.InvalidValue
            )
        )
        assertEquals(
            4,
            labeledCounterMetric["__other__"].testGetValue()
        )
    }

    @Test
    fun `Test labeled timespan metric type`() {
        TimespansStorageEngine.clearAllStores()

        val timespanMetric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_timespan_metric",
            sendInPings = listOf("default"),
            timeUnit = TimeUnit.Nanosecond
        )

        val labeledTimespanMetric = LabeledMetricType<TimespanMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_timespan_metric",
            sendInPings = listOf("default"),
            subMetric = timespanMetric
        )

        TimespansStorageEngine.start(labeledTimespanMetric["label1"])
        TimespansStorageEngine.stopAndSum(labeledTimespanMetric["label1"], TimeUnit.Nanosecond)
        TimespansStorageEngine.start(labeledTimespanMetric["label2"])
        TimespansStorageEngine.stopAndSum(labeledTimespanMetric["label2"], TimeUnit.Nanosecond)

        collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
    }

    @Test
    fun `Test labeled uuid metric type`() {
        val uuidMetric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_uuid_metric",
            sendInPings = listOf("default")
        )

        val labeledUuidMetric = LabeledMetricType<UuidMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_uuid_metric",
            sendInPings = listOf("default"),
            subMetric = uuidMetric
        )

        UuidsStorageEngine.record(labeledUuidMetric["label1"], UUID.randomUUID())
        UuidsStorageEngine.record(labeledUuidMetric["label2"], UUID.randomUUID())

        collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
    }

    @Test
    fun `Test labeled string list metric type`() {
        StringListsStorageEngine.clearAllStores()

        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_list_metric",
            sendInPings = listOf("default")
        )

        val labeledStringListMetric = LabeledMetricType<StringListMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_list_metric",
            sendInPings = listOf("default"),
            subMetric = stringListMetric
        )

        StringListsStorageEngine.set(labeledStringListMetric["label1"], listOf("a", "b", "c"))
        StringListsStorageEngine.set(labeledStringListMetric["label2"], listOf("a", "b", "c"))

        collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
    }

    @Test
    fun `Test labeled string metric type`() {
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_metric",
            sendInPings = listOf("default")
        )

        val labeledStringMetric = LabeledMetricType<StringMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_metric",
            sendInPings = listOf("default"),
            subMetric = stringMetric
        )

        StringsStorageEngine.record(labeledStringMetric["label1"], "foo")
        StringsStorageEngine.record(labeledStringMetric["label2"], "bar")

        collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
    }

    @Test
    fun `Test labeled boolean metric type`() {
        BooleansStorageEngine.clearAllStores()

        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_list_metric",
            sendInPings = listOf("default")
        )

        val labeledBooleanMetric = LabeledMetricType<BooleanMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_string_list_metric",
            sendInPings = listOf("default"),
            subMetric = booleanMetric
        )

        BooleansStorageEngine.record(labeledBooleanMetric["label1"], false)
        BooleansStorageEngine.record(labeledBooleanMetric["label2"], true)

        collectAndCheckPingSchema("metrics").getJSONObject("metrics")!!
    }

    @Test(expected = IllegalStateException::class)
    fun `Test that we labeled events are an exception`() {
        val eventMetric = EventMetricType<NoExtraKeys>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_event_metric",
            sendInPings = listOf("default")
        )

        val labeledEventMetric = LabeledMetricType<EventMetricType<NoExtraKeys>>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "labeled_event_metric",
            sendInPings = listOf("default"),
            subMetric = eventMetric
        )

        labeledEventMetric["label1"]
    }

    @Test
    fun `test seen labels get reloaded from disk`() {
        val persistedSample = mapOf(
            "store1#telemetry.labeled_metric/label1" to 1,
            "store1#telemetry.labeled_metric/label2" to 1,
            "store1#telemetry.labeled_metric/label3" to 1
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

        val storageEngine = MockGenericStorageEngine()
        storageEngine.applicationContext = context

        val metric = GenericMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "labeled_metric",
            sendInPings = listOf("store1")
        )

        val labeledMetric = LabeledMetricType<GenericMetricType>(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "labeled_metric",
            sendInPings = listOf("store1"),
            subMetric = metric
        )

        val labeledMetricSpy = spy(labeledMetric)
        doReturn(storageEngine).`when`(labeledMetricSpy).getStorageEngineForMetric()

        doAnswer {
            metric.copy(name = it.arguments[0] as String)
        }.`when`(labeledMetricSpy).getMetricWithNewName(anyString())

        for (i in 4..20) {
            storageEngine.record(labeledMetricSpy["label$i"], 1)
        }
        // Go back and record in one of the real labels again
        storageEngine.record(labeledMetricSpy["label1"], 2)

        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = false)

        assertEquals(17, snapshot!!.size)
        assertEquals(2, snapshot.get("telemetry.labeled_metric/label1"))
        for (i in 2..15) {
            assertEquals(1, snapshot.get("telemetry.labeled_metric/label$i"))
        }
        assertEquals(1, snapshot.get("telemetry.labeled_metric/__other__"))
    }
}
