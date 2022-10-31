/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config
import java.security.Security

@RunWith(AndroidJUnit4::class)
class SecureAbove22PreferencesTest {
    @Config(sdk = [21])
    @Test
    fun `CRUD tests API level 21 unencrypted`() {
        val storage = SecureAbove22Preferences(testContext, "hello")
        val storage2 = SecureAbove22Preferences(testContext, "world")

        // no keys
        assertNull(storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // single key
        storage.putString("hello", "world")
        assertEquals("world", storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // single key, updated
        storage.putString("hello", "you")
        assertEquals("you", storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // multiple keys
        storage.putString("test", "string")
        assertEquals("string", storage.getString("test"))
        assertEquals("you", storage.getString("hello"))
        val all = storage.all()
        assertEquals(2, all.size)
        assertEquals("string", all["test"])
        assertEquals("you", all["hello"])
        assertTrue(storage2.all().isEmpty())

        // clearing one storage doesn't affect another with a different name
        storage2.putString("another", "test")
        assertEquals(1, storage2.all().size)
        storage2.clear()
        assertEquals(2, storage.all().size)

        // key removal
        storage.remove("hello")
        assertNull(storage.getString("hello"))
        storage.remove("test")
        assertNull(storage.getString("test"))
        assertTrue(storage2.all().isEmpty())

        // clearing
        storage.putString("one", "two")
        assertEquals("two", storage.getString("one"))
        storage.putString("three", "four")
        assertEquals("four", storage.getString("three"))
        storage.putString("five", "six")
        assertEquals("six", storage.getString("five"))
        assertTrue(storage2.all().isEmpty())

        storage.clear()
        assertNull(storage.getString("one"))
        assertNull(storage.getString("three"))
        assertNull(storage.getString("five"))
        assertTrue(storage.all().isEmpty())
        assertTrue(storage2.all().isEmpty())
    }

    @Config(sdk = [22])
    @Test
    fun `CRUD tests API level 22 unencrypted`() {
        val storage = SecureAbove22Preferences(testContext, "hello")
        val storage2 = SecureAbove22Preferences(testContext, "world")

        // no keys
        assertNull(storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // single key
        storage.putString("hello", "world")
        assertEquals("world", storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // single key, updated
        storage.putString("hello", "you")
        assertEquals("you", storage.getString("hello"))
        assertTrue(storage2.all().isEmpty())

        // multiple keys
        storage.putString("test", "string")
        assertEquals("string", storage.getString("test"))
        assertEquals("you", storage.getString("hello"))
        val all = storage.all()
        assertEquals(2, all.size)
        assertEquals("string", all["test"])
        assertEquals("you", all["hello"])
        assertTrue(storage2.all().isEmpty())

        // clearing one storage doesn't affect another with a different name
        storage2.putString("another", "test")
        assertEquals(1, storage2.all().size)
        storage2.clear()
        assertEquals(2, storage.all().size)

        // key removal
        storage.remove("hello")
        assertNull(storage.getString("hello"))
        storage.remove("test")
        assertNull(storage.getString("test"))
        assertTrue(storage2.all().isEmpty())

        // clearing
        storage.putString("one", "two")
        assertEquals("two", storage.getString("one"))
        storage.putString("three", "four")
        assertEquals("four", storage.getString("three"))
        storage.putString("five", "six")
        assertEquals("six", storage.getString("five"))
        assertTrue(storage2.all().isEmpty())

        storage.clear()
        assertNull(storage.getString("one"))
        assertNull(storage.getString("three"))
        assertNull(storage.getString("five"))
        assertTrue(storage.all().isEmpty())
        assertTrue(storage2.all().isEmpty())
    }

    @Config(sdk = [21])
    @Test
    fun `storage instances of the same name are interchangeable`() {
        val storage = SecureAbove22Preferences(testContext, "hello")
        val storage2 = SecureAbove22Preferences(testContext, "hello")

        storage.putString("key1", "value1")
        assertEquals("value1", storage2.getString("key1"))

        storage2.putString("something", "other")
        assertEquals("other", storage.getString("something"))

        assertEquals(storage.all().size, storage2.all().size)
        assertEquals(storage.all(), storage2.all())

        storage.clear()
        assertTrue(storage2.all().isEmpty())
    }

    @Ignore("https://github.com/mozilla-mobile/android-components/issues/4956")
    @Config(sdk = [23])
    @Test
    fun `CRUD tests API level 23+ encrypted`() {
        // TODO find out what this is; lockwise tests set it.
        Security.setProperty("crypto.policy", "unlimited")

        val storage = SecureAbove22Preferences(testContext, "test")

        // no keys
        assertNull(storage.getString("hello"))

        // single key
        storage.putString("hello", "world")
        assertEquals("world", storage.getString("hello"))

        // single key, updated
        storage.putString("hello", "you")
        assertEquals("you", storage.getString("hello"))

        // multiple keys
        storage.putString("test", "string")
        assertEquals("string", storage.getString("test"))
        assertEquals("you", storage.getString("hello"))

        // key removal
        storage.remove("hello")
        assertNull(storage.getString("hello"))
        storage.remove("test")
        assertNull(storage.getString("test"))
    }
}
