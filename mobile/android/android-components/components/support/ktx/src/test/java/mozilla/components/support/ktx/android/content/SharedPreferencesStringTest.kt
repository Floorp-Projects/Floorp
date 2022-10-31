/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.Context
import android.content.SharedPreferences
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SharedPreferencesStringTest {
    private val key = "key"
    private val defaultValue = "defaultString"
    private lateinit var preferencesHolder: StringTestPreferenceHolder
    private lateinit var testPreferences: SharedPreferences

    @Before
    fun setup() {
        testPreferences = testContext.getSharedPreferences("test", Context.MODE_PRIVATE)
    }

    @After
    fun tearDown() {
        testPreferences.edit().clear().apply()
    }

    @Test
    fun `GIVEN string does not exist and asked to persist the default WHEN asked for it THEN persist the default and return it`() {
        preferencesHolder = StringTestPreferenceHolder(
            persistDefaultIfNotExists = true,
        )

        val result = preferencesHolder.string

        assertEquals(defaultValue, result)
        assertEquals(defaultValue, testPreferences.getString(key, null))
    }

    @Test
    fun `GIVEN string does not exist and not asked to persist the default WHEN asked for it THEN return the default but not persist it`() {
        preferencesHolder = StringTestPreferenceHolder(
            persistDefaultIfNotExists = false,
        )

        val result = preferencesHolder.string

        assertEquals(defaultValue, result)
        assertNull(testPreferences.getString(key, null))
    }

    @Test
    fun `GIVEN string exists and asked to persist the default WHEN asked for it THEN return the existing string and don't persist the default`() {
        testPreferences.edit().putString(key, "test").apply()
        preferencesHolder = StringTestPreferenceHolder(
            persistDefaultIfNotExists = true,
        )

        val result = preferencesHolder.string

        assertEquals("test", result)
    }

    @Test
    fun `GIVEN string exists and not asked to persist the default WHEN asked for it THEN return the existing string and don't persist the default`() {
        testPreferences.edit().putString(key, "test").apply()
        preferencesHolder = StringTestPreferenceHolder(
            persistDefaultIfNotExists = true,
        )

        val result = preferencesHolder.string

        assertEquals("test", result)
    }

    @Test
    fun `GIVEN a value exists WHEN asked to persist a new value THEN update the persisted value`() {
        testPreferences.edit().putString(key, "test").apply()
        preferencesHolder = StringTestPreferenceHolder()

        preferencesHolder.string = "update"

        assertEquals(
            "update",
            testPreferences.getString(key, null),
        )
    }

    @Test
    fun `GIVEN a value does not exist WHEN asked to persist a new value THEN persist the requested value`() {
        preferencesHolder = StringTestPreferenceHolder()

        preferencesHolder.string = "test"

        assertEquals(
            "test",
            testPreferences.getString(key, null),
        )
    }

    private inner class StringTestPreferenceHolder(
        persistDefaultIfNotExists: Boolean = false,
    ) : PreferencesHolder {
        override val preferences = testPreferences

        var string by stringPreference(key, defaultValue, persistDefaultIfNotExists)
    }
}
