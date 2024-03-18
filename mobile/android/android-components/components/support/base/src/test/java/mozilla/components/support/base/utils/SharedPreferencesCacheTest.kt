/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SharedPreferencesCacheTest {
    data class TestType(val someKey: String, val anotherKey: Long)

    class TestTypeCache : SharedPreferencesCache<TestType>(testContext) {
        override val logger: Logger = Logger("test")
        override val cacheKey: String = "testKey"
        override val cacheName: String = "testName"

        override fun TestType.toJSON(): JSONObject {
            return JSONObject().apply {
                put("someKey", someKey)
                put("anotherKey", anotherKey)
            }
        }

        override fun fromJSON(obj: JSONObject): TestType {
            return TestType(obj.getString("someKey"), obj.getLong("anotherKey"))
        }
    }

    @Test
    fun `shared preferences cache basics`() {
        val cache = TestTypeCache()
        assertNull(cache.getCached())
        cache.setToCache(TestType("hi", 177))
        assertEquals(TestType("hi", 177), cache.getCached())

        // Corrupt the cache...
        testContext.getSharedPreferences("testName", Context.MODE_PRIVATE).edit()
            .putString("testKey", "garbage").commit()

        // Should handle bad data as 'no cached values'.
        assertNull(cache.getCached())

        // Should be able to persist new data after corruption.
        cache.setToCache(TestType("test", 1337))
        assertEquals(TestType("test", 1337), cache.getCached())

        // Should be able to clear cache.
        cache.clear()
        assertNull(cache.getCached())
    }
}
