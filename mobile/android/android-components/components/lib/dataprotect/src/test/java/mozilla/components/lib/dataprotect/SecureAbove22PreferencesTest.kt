/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
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
        val storage = SecureAbove22Preferences(testContext)

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

    @Config(sdk = [22])
    @Test
    fun `CRUD tests API level 22 unencrypted`() {
        val storage = SecureAbove22Preferences(testContext)

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

    @Ignore("https://github.com/mozilla-mobile/android-components/issues/4956")
    @Config(sdk = [23])
    @Test
    fun `CRUD tests API level 23+ encrypted`() {
        // TODO find out what this is; lockwise tests set it.
        Security.setProperty("crypto.policy", "unlimited")

        val storage = SecureAbove22Preferences(testContext)

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