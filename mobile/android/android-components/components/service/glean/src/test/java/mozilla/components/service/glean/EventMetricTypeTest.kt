/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import mozilla.components.service.glean.storages.EventsStorageEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

import org.junit.Before
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.shadows.ShadowSystemClock

@RunWith(RobolectricTestRunner::class)
class EventMetricTypeTest {

    @Before
    fun setUp() {
        EventsStorageEngine.clearAllStores()
    }

    @Test
    fun `The API records to its storage engine`() {
        // Define a 'click' event, which will be stored in "store1"
        val click = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        // Record two events of the same type, with a little delay.
        click.record("buttonA")

        val expectedTimeSinceStart: Long = 37
        ShadowSystemClock.sleep(expectedTimeSinceStart)

        click.record("buttonB")

        // Check that data was properly recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(2, snapshot!!.size)

        val firstEvent = snapshot.filter { e -> e.objectId == "buttonA" }.single()
        assertEquals("ui", firstEvent.category)
        assertEquals("click", firstEvent.name)

        val secondEvent = snapshot.filter { e -> e.objectId == "buttonB" }.single()
        assertEquals("ui", secondEvent.category)
        assertEquals("click", secondEvent.name)

        assertTrue("The sequence of the events must be preserved",
            firstEvent.msSinceStart < secondEvent.msSinceStart)
    }

    @Test
    fun `events with no lifetime must not record data`() {
        // Define a 'click' event, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val click = EventMetricType(
            applicationProperty = false,
            disabled = true,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        // Attempt to store the event.
        click.record("buttonA")

        // Check that nothing was recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Events must not be recorded if they are disabled", snapshot)
    }

    @Test
    fun `disabled events must not record data`() {
        // Define a 'click' event, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val click = EventMetricType(
            applicationProperty = true,
            disabled = true,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        // Attempt to store the event.
        click.record("buttonA")

        // Check that nothing was recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Events must not be recorded if they are disabled", snapshot)
    }

    @Test
    fun `'objectId' is properly recorded and truncated`() {
        val click = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        val testValue = "LeanGleanByFrank"
        click.record(testValue)
        click.record(testValue.repeat(3))

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)

        val firstEvent = snapshot!!.filter { e -> e.objectId == testValue }.single()
        assertEquals("ui", firstEvent.category)
        assertEquals("click", firstEvent.name)

        val secondEvent = snapshot.filter {
            e -> e.objectId == testValue.repeat(3).substring(0, MAX_LENGTH_OBJECT_ID)
        }.single()
        assertEquals("ui", secondEvent.category)
        assertEquals("click", secondEvent.name)
    }

    @Test
    fun `'value' is properly recorded and truncated`() {
        val click = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        val testValue = "LeanGleanByFrank"
        click.record("buttonA", value = testValue)
        click.record("buttonB", value = testValue.repeat(10))

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)

        val firstEvent = snapshot!!.filter { e -> e.objectId == "buttonA" }.single()
        assertEquals("ui", firstEvent.category)
        assertEquals("click", firstEvent.name)
        assertEquals(testValue, firstEvent.value)

        val secondEvent = snapshot.filter { e -> e.objectId == "buttonB" }.single()
        assertEquals("ui", secondEvent.category)
        assertEquals("click", secondEvent.name)
        assertEquals(testValue.repeat(10).substring(0, MAX_LENGTH_VALUE), secondEvent.value)
    }

    @Test
    fun `using 'extra' without declaring allowed keys must not be recorded`() {
        val testEvent = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "testEvent",
            sendInPings = listOf("store1"),
            userProperty = false
        )

        testEvent.record("buttonA",
            extra = mapOf("unknownExtra" to "someValue", "unknownExtra2" to "test"))

        // Check that nothing was recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Events must not be recorded if they use unknown extra keys", snapshot)
    }

    @Test
    fun `unknown 'extra' keys must not be recorded`() {
        val testEvent = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "testEvent",
            sendInPings = listOf("store1"),
            userProperty = false,
            allowedExtraKeys = listOf("extra1", "extra2")
        )

        testEvent.record("buttonA",
            extra = mapOf("unknownExtra" to "someValue", "extra1" to "test"))

        // Check that nothing was recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Events must not be recorded if they use unknown extra keys", snapshot)
    }

    @Test
    fun `'extra' keys must be recorded and truncated if needed`() {
        val testEvent = EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "testEvent",
            sendInPings = listOf("store1"),
            userProperty = false,
            allowedExtraKeys = listOf("extra1", "truncatedExtra")
        )

        val testValue = "LeanGleanByFrank"
        testEvent.record("buttonA",
            extra = mapOf("extra1" to testValue, "truncatedExtra" to testValue.repeat(10)))

        // Check that nothing was recorded.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals("ui", snapshot.first().category)
        assertEquals("testEvent", snapshot.first().name)

        assertTrue(
            "'extra' keys must be correctly recorded and truncated",
            mapOf(
                "extra1" to testValue,
                "truncatedExtra" to (testValue.repeat(10)).substring(0, MAX_LENGTH_EXTRA_KEY_VALUE)
            ).equals(snapshot.first().extra))
    }
}
