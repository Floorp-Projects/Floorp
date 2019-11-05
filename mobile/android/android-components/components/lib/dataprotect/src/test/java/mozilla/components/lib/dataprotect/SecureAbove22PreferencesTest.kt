/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import android.content.Context.MODE_PRIVATE
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

        // clearing
        storage.putString("one", "two")
        assertEquals("two", storage.getString("one"))
        storage.putString("three", "four")
        assertEquals("four", storage.getString("three"))
        storage.putString("five", "six")
        assertEquals("six", storage.getString("five"))

        storage.clear()
        assertNull(storage.getString("one"))
        assertNull(storage.getString("three"))
        assertNull(storage.getString("five"))
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

        // clearing
        storage.putString("one", "two")
        assertEquals("two", storage.getString("one"))
        storage.putString("three", "four")
        assertEquals("four", storage.getString("three"))
        storage.putString("five", "six")
        assertEquals("six", storage.getString("five"))

        storage.clear()
        assertNull(storage.getString("one"))
        assertNull(storage.getString("three"))
        assertNull(storage.getString("five"))
    }

    @Config(sdk = [22])
    @Test
    fun `pref migration`() {
        // NB: this test is targeting 22, even though we're testing 22->23 migration scenario.
        // Internally, `migratePrefs` simply writes values from the provided prefs into current prefs. If our encryption
        // roundtrip tests worked, we could have tests that for sdk=23, but they do not (see https://github.com/mozilla-mobile/android-components/issues/4956),
        // and so we're just testing this backed by regular SharedPreferences.
        // This covers the core logic of migration, so it's an acceptable "hack".

        val plainPrefs = testContext.getSharedPreferences("test", MODE_PRIVATE)
        val storage = SecureAbove22Preferences(testContext)

        // can migrate empty prefs
        storage.migratePrefs(plainPrefs)

        // can migrate mixed prefs
        plainPrefs
            .edit()
            .putString("hello", "world")
            .putBoolean("some_flag", false)
            .putString("another", "string")
            .putInt("counter", 42)
            .commit()

        storage.migratePrefs(plainPrefs)

        assertEquals("world", storage.getString("hello"))
        assertEquals("string", storage.getString("another"))

        // original prefs empty after migrating
        assertEquals(0, plainPrefs.all.size)
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