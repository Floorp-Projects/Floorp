/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Intent
import android.os.Parcelable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import java.util.HashSet

@RunWith(AndroidJUnit4::class)
class SafeIntentTest {

    private lateinit var intent: Intent

    @Before
    fun setup() {
        intent = mock()
    }

    @Test
    fun `getStringArrayListExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getStringArrayListExtra(anyString())

        assertNull(SafeIntent(intent).getStringArrayListExtra("mozilla"))
    }

    @Test
    fun `getExtras returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).extras

        assertNull(SafeIntent(intent).extras)
    }

    @Test
    fun `getAction return original action`() {
        val expected = Intent.ACTION_MAIN

        doReturn(expected)
            .`when`(intent).action

        assertEquals(expected, SafeIntent(intent).action)
    }

    @Test
    fun `getFlags returns original flags`() {
        val expected = Intent.FLAG_ACTIVITY_NEW_TASK

        doReturn(expected)
            .`when`(intent).flags

        assertEquals(expected, SafeIntent(intent).flags)
    }

    @Test
    fun `isLauncherIntent returns false if intent is not Launcher Intent`() {
        // category is null
        doReturn(null)
            .`when`(intent).categories

        assertFalse(SafeIntent(intent).isLauncherIntent)

        // both category and action are not valid
        val category = HashSet<String>()

        category.add("NOT" + Intent.CATEGORY_LAUNCHER)

        doReturn(category)
            .`when`(intent).categories

        doReturn("NOT" + Intent.ACTION_MAIN)
            .`when`(intent).action

        assertFalse(SafeIntent(intent).isLauncherIntent)

        // action is not valid
        category.clear()

        category.add(Intent.CATEGORY_LAUNCHER)

        doReturn(category)
            .`when`(intent).categories

        doReturn("NOT" + Intent.ACTION_MAIN)
            .`when`(intent).action

        assertFalse(SafeIntent(intent).isLauncherIntent)

        // category is not valid
        category.clear()

        category.add("NOT" + Intent.CATEGORY_LAUNCHER)

        doReturn(category)
            .`when`(intent).categories

        doReturn(Intent.ACTION_MAIN)
            .`when`(intent).action

        assertFalse(SafeIntent(intent).isLauncherIntent)

        // both are valid
        category.clear()

        category.add(Intent.CATEGORY_LAUNCHER)

        doReturn(category)
            .`when`(intent).categories

        doReturn(Intent.ACTION_MAIN)
            .`when`(intent).action

        assertTrue(SafeIntent(intent).isLauncherIntent)
    }

    @Test
    fun `isLauncherIntent returns true if intent is Launcher Intent`() {
        // both category and action are not valid
        val category = HashSet<String>()
        category.add(Intent.CATEGORY_LAUNCHER)

        doReturn(category)
            .`when`(intent).categories

        doReturn(Intent.ACTION_MAIN)
            .`when`(intent).action

        assertTrue(SafeIntent(intent).isLauncherIntent)
    }

    @Test
    fun `getDataString returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).dataString

        assertNull(SafeIntent(intent).dataString)
    }

    @Test
    fun `getData returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).data

        assertNull(SafeIntent(intent).data)
    }

    @Test
    fun `getCategories returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).categories

        assertNull(SafeIntent(intent).categories)
    }

    @Test
    fun `hasExtra returns false if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).hasExtra(anyString())

        assertFalse(SafeIntent(intent).hasExtra(""))
    }

    @Test
    fun `getBooleanExtra returns false if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getBooleanExtra(anyString(), anyBoolean())

        assertFalse(SafeIntent(intent).getBooleanExtra("", false))
    }

    @Test
    fun `getIntExtra returns default value if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getIntExtra(anyString(), anyInt())

        val expected = 1
        assertEquals(expected, SafeIntent(intent).getIntExtra("", expected))
    }

    @Test
    fun `getStringExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getStringExtra(anyString())

        assertNull(SafeIntent(intent).getStringExtra(""))
    }

    @Test
    fun `getBundleExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getBundleExtra(anyString())

        assertNull(SafeIntent(intent).getBundleExtra(""))
    }

    @Test
    fun `getCharSequenceExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getCharSequenceExtra(anyString())

        assertNull(SafeIntent(intent).getCharSequenceExtra(""))
    }

    @Test
    fun `getParcelableExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getParcelableExtra<Parcelable>(anyString())

        assertNull(SafeIntent(intent).getParcelableExtra(""))
    }

    @Test
    fun `getParcelableArrayListExtra returns null if intent throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java)
            .`when`(intent).getParcelableArrayListExtra<Parcelable>(anyString())

        assertNull(SafeIntent(intent).getParcelableArrayListExtra<Parcelable>(""))
    }

    @Test
    fun `getParcelableArrayListExtra returns ArrayList if intent is safe`() {
        val expected = ArrayList<Any>()
        doReturn(expected)
            .`when`(intent).getParcelableArrayListExtra<Parcelable>(anyString())

        assertEquals(expected, SafeIntent(intent).getParcelableArrayListExtra<Parcelable>(""))
    }

    @Test
    fun `getUnsafe returns original intent`() {
        assertEquals(intent, SafeIntent(intent).unsafe)
    }

    @Test
    fun `WHEN toSafeIntent wraps an intent THEN it has the same unsafe intent as the SafeIntent constructor`() {
        // SafeIntent does not override .equals so we have to do comparison with their underlying unsafe intents.
        assertEquals(SafeIntent(intent).unsafe, intent.toSafeIntent().unsafe)
    }
}
