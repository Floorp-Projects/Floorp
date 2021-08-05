/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.SharedPreferences
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyString
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.openMocks

class SharedPreferencesTest {
    @Mock private lateinit var sharedPrefs: SharedPreferences
    @Mock private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        openMocks(this)

        `when`(sharedPrefs.edit()).thenReturn(editor)
        `when`(editor.putBoolean(anyString(), anyBoolean())).thenReturn(editor)
        `when`(editor.putFloat(anyString(), anyFloat())).thenReturn(editor)
        `when`(editor.putInt(anyString(), anyInt())).thenReturn(editor)
        `when`(editor.putLong(anyString(), anyLong())).thenReturn(editor)
        `when`(editor.putString(anyString(), anyString())).thenReturn(editor)
        `when`(editor.putStringSet(anyString(), any())).thenReturn(editor)
    }

    @Test
    fun `getter returns boolean from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getBoolean(eq("boolean"), anyBoolean())).thenReturn(true)

        assertTrue(holder.boolean)
        verify(sharedPrefs).getBoolean("boolean", false)
    }

    @Test
    fun `setter applies boolean to shared preferences`() {
        val holder = MockPreferencesHolder(defaultBoolean = true)
        holder.boolean = false

        verify(editor).putBoolean("boolean", false)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default boolean value`() {
        val holderFalse = MockPreferencesHolder(defaultBoolean = false)
        // Call the getter for the test
        holderFalse.boolean

        verify(sharedPrefs).getBoolean("boolean", false)

        val holderTrue = MockPreferencesHolder(defaultBoolean = true)
        // Call the getter for the test
        holderTrue.boolean

        verify(sharedPrefs).getBoolean("boolean", true)
    }

    @Test
    fun `getter returns float from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getFloat(eq("float"), anyFloat())).thenReturn(2.4f)

        assertEquals(2.4f, holder.float)
        verify(sharedPrefs).getFloat("float", 0f)
    }

    @Test
    fun `setter applies float to shared preferences`() {
        val holder = MockPreferencesHolder(defaultFloat = 1f)
        holder.float = 0f

        verify(editor).putFloat("float", 0f)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default float value`() {
        val holderDefault = MockPreferencesHolder(defaultFloat = 0f)
        // Call the getter for the test
        holderDefault.float

        verify(sharedPrefs).getFloat("float", 0f)

        val holderOther = MockPreferencesHolder(defaultFloat = 12f)
        // Call the getter for the test
        holderOther.float

        verify(sharedPrefs).getFloat("float", 12f)
    }

    @Test
    fun `getter returns int from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getInt(eq("int"), anyInt())).thenReturn(5)

        assertEquals(5, holder.int)
        verify(sharedPrefs).getInt("int", 0)
    }

    @Test
    fun `setter applies int to shared preferences`() {
        val holder = MockPreferencesHolder(defaultInt = 1)
        holder.int = 0

        verify(editor).putInt("int", 0)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default int value`() {
        val holderDefault = MockPreferencesHolder(defaultInt = 0)
        // Call the getter for the test
        holderDefault.int

        verify(sharedPrefs).getInt("int", 0)

        val holderOther = MockPreferencesHolder(defaultInt = 23)
        // Call the getter for the test
        holderOther.int

        verify(sharedPrefs).getInt("int", 23)
    }

    @Test
    fun `getter returns long from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getLong(eq("long"), anyLong())).thenReturn(200L)

        assertEquals(200L, holder.long)
        verify(sharedPrefs).getLong("long", 0)
    }

    @Test
    fun `setter applies long to shared preferences`() {
        val holder = MockPreferencesHolder(defaultLong = 1)
        holder.long = 0

        verify(editor).putLong("long", 0)
        verify(editor).apply()
    }

    @Test
    fun `getter uses default long value`() {
        val holderDefault = MockPreferencesHolder(defaultLong = 0)
        // Call the getter for the test
        holderDefault.long

        verify(sharedPrefs).getLong("long", 0)

        val holderOther = MockPreferencesHolder(defaultLong = 23)
        // Call the getter for the test
        holderOther.long

        verify(sharedPrefs).getLong("long", 23)
    }

    @Test
    fun `getter returns string from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getString(eq("string"), anyString())).thenReturn("foo")

        assertEquals("foo", holder.string)
        verify(sharedPrefs).getString("string", "")
    }

    @Test
    fun `setter applies string to shared preferences`() {
        val holder = MockPreferencesHolder(defaultString = "foo")
        holder.string = "bar"

        verify(editor).putString("string", "bar")
        verify(editor).apply()
    }

    @Test
    fun `getter uses default string value`() {
        val holderDefault = MockPreferencesHolder()
        // Call the getter for the test
        holderDefault.string

        verify(sharedPrefs).getString("string", "")

        val holderOther = MockPreferencesHolder(defaultString = "hello")
        // Call the getter for the test
        holderOther.string

        verify(sharedPrefs).getString("string", "hello")
    }

    @Test
    fun `getter returns string set from shared preferences`() {
        val holder = MockPreferencesHolder()
        `when`(sharedPrefs.getStringSet(eq("string_set"), any())).thenReturn(setOf("foo"))

        assertEquals(setOf("foo"), holder.stringSet)
        verify(sharedPrefs).getStringSet("string_set", emptySet())
    }

    @Test
    fun `setter applies string set to shared preferences`() {
        val holder = MockPreferencesHolder(defaultString = "foo")
        holder.stringSet = setOf("bar")

        verify(editor).putStringSet("string_set", setOf("bar"))
        verify(editor).apply()
    }

    @Test
    fun `getter uses default string set value`() {
        val holderDefault = MockPreferencesHolder()
        // Call the getter for the test
        holderDefault.stringSet

        verify(sharedPrefs).getStringSet("string_set", emptySet())

        val holderOther = MockPreferencesHolder(defaultSet = setOf("hello", "world"))
        // Call the getter for the test
        holderOther.stringSet

        verify(sharedPrefs).getStringSet("string_set", setOf("hello", "world"))
    }

    private inner class MockPreferencesHolder(
        defaultBoolean: Boolean = false,
        defaultFloat: Float = 0f,
        defaultInt: Int = 0,
        defaultLong: Long = 0L,
        defaultString: String = "",
        defaultSet: Set<String> = emptySet()
    ) : PreferencesHolder {
        override val preferences = sharedPrefs

        var boolean by booleanPreference("boolean", default = defaultBoolean)

        var float by floatPreference("float", default = defaultFloat)

        var int by intPreference("int", default = defaultInt)

        var long by longPreference("long", default = defaultLong)

        var string by stringPreference("string", default = defaultString)

        var stringSet by stringSetPreference("string_set", default = defaultSet)
    }
}
