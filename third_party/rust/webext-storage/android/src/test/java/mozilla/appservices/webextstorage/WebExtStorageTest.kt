/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.appservices.webextstorage

import mozilla.appservices.Megazord
import org.junit.Assert
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE)
class WebExtStorageTest {
    @Rule
    @JvmField
    val dbFolder = TemporaryFolder()

    fun createTestStore(): WebExtStorageStore {
        Megazord.init()
        val dbPath = dbFolder.newFile()
        return WebExtStorageStore(path = dbPath.absolutePath)
    }

    @Test
    fun testSet() {
        val store = createTestStore()
        val extId = "ab"
        val jsonString = """{"a":"a"}"""

        store.set(extId, jsonString)
        val data = store.get(extId, "null")
        Assert.assertEquals(jsonString, data)
        store.close()
    }

    @Test
    fun testRemove() {
        val store = createTestStore()
        val extId = "ab"
        val jsonString = """{"a":"a","b":"b"}"""

        store.set(extId, jsonString)
        val change = store.remove("ab", """["b"]""").changes[0]

        Assert.assertEquals(change.key, "b")
        Assert.assertEquals(change.oldValue, """"b"""")
        Assert.assertNull(change.newValue)
        store.close()
    }

    @Test
    fun testClear() {
        val store = createTestStore()
        val extId = "ab"
        val jsonString = """{"a":"a","b":"b"}"""

        store.set(extId, jsonString)
        val result = store.clear(extId)

        val firstChange = result.changes[0]
        Assert.assertEquals(firstChange.key, "a")
        Assert.assertEquals(firstChange.oldValue, """"a"""")
        Assert.assertNull(firstChange.newValue)

        val secondChange = result.changes[1]
        Assert.assertEquals(secondChange.key, "b")
        Assert.assertEquals(secondChange.oldValue, """"b"""")
        Assert.assertNull(secondChange.newValue)

        store.close()
    }
}
