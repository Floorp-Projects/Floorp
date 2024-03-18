/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
@kotlinx.coroutines.ExperimentalCoroutinesApi
class OnDiskMessageMetadataStorageTest {

    private lateinit var storage: OnDiskMessageMetadataStorage

    @Before
    fun setup() {
        storage = OnDiskMessageMetadataStorage(
            testContext,
        )
    }

    @Test
    fun `GIVEN metadata is not loaded from disk WHEN calling getMetadata THEN load it`() =
        runTest {
            val spiedStorage = spy(storage)

            `when`(spiedStorage.readFromDisk()).thenReturn(emptyMap())

            spiedStorage.getMetadata()

            verify(spiedStorage).readFromDisk()
        }

    @Test
    fun `GIVEN metadata is loaded from disk WHEN calling getMetadata THEN do not load it from disk`() =
        runTest {
            val spiedStorage = spy(storage)

            spiedStorage.metadataMap = hashMapOf("" to Message.Metadata("id"))

            spiedStorage.getMetadata()

            verify(spiedStorage, never()).readFromDisk()
        }

    @Test
    fun `WHEN calling addMetadata THEN add in memory and disk`() = runTest {
        val spiedStorage = spy(storage)

        assertTrue(spiedStorage.metadataMap.isEmpty())

        `when`(spiedStorage.writeToDisk()).then { }

        spiedStorage.addMetadata(Message.Metadata("id"))

        assertFalse(spiedStorage.metadataMap.isEmpty())
        verify(spiedStorage).writeToDisk()
    }

    @Test
    fun `WHEN calling updateMetadata THEN delegate to addMetadata`() = runTest {
        val spiedStorage = spy(storage)
        val metadata = Message.Metadata("id")
        `when`(spiedStorage.writeToDisk()).then { }

        spiedStorage.updateMetadata(metadata)

        verify(spiedStorage).addMetadata(metadata)
    }

    @Test
    fun `WHEN calling toJson THEN return an string json representation`() {
        val metadata = Message.Metadata(
            id = "id",
            displayCount = 1,
            pressed = false,
            dismissed = false,
            lastTimeShown = 0L,
            latestBootIdentifier = "9",
        )

        val expected =
            """{"id":"id","displayCount":1,"pressed":false,"dismissed":false,"lastTimeShown":0,"latestBootIdentifier":"9"}"""

        assertEquals(expected, metadata.toJson())
    }

    @Test
    fun `WHEN calling toMetadata THEN return Metadata representation`() {
        val json =
            """{"id":"id","displayCount":1,"pressed":false,"dismissed":false,"lastTimeShown":0,"latestBootIdentifier":"9"}"""

        val jsonObject = JSONObject(json)

        val metadata = Message.Metadata(
            id = "id",
            displayCount = 1,
            pressed = false,
            dismissed = false,
            lastTimeShown = 0L,
            latestBootIdentifier = "9",
        )

        assertEquals(metadata, jsonObject.toMetadata())
    }

    @Test
    fun `WHEN calling toMetadataMap THEN return map representation`() {
        val json =
            """[{"id":"id","displayCount":1,"pressed":false,"dismissed":false,"lastTimeShown":0,"latestBootIdentifier":"9"}]"""

        val jsonArray = JSONArray(json)

        val metadata = Message.Metadata(
            id = "id",
            displayCount = 1,
            pressed = false,
            dismissed = false,
            lastTimeShown = 0L,
            latestBootIdentifier = "9",
        )

        assertEquals(metadata, jsonArray.toMetadataMap()[metadata.id])
    }
}
