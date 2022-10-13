/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.Bundle
import android.os.Parcelable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doThrow

@RunWith(AndroidJUnit4::class)
class SafeBundleTest {

    private lateinit var bundle: Bundle

    @Before
    fun setup() {
        bundle = mock()
    }

    @Test
    fun `getInt returns default value if bundle throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java).`when`(bundle).getInt(anyString(), anyInt())

        val expected = 1
        assertEquals(expected, SafeBundle(bundle).getInt("", expected))
    }

    @Test
    fun `getString returns null if bundle throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java).`when`(bundle).getString(anyString())

        assertNull(bundle.toSafeBundle().getString(""))
    }

    @Test
    fun `getParcelable returns null if bundle throws OutOfMemoryError`() {
        doThrow(OutOfMemoryError::class.java).`when`(bundle).getParcelable<Parcelable>(anyString())

        assertNull(SafeBundle(bundle).getParcelable(""))
    }

    @Test
    fun `getUnsafe returns original bundle`() {
        assertEquals(bundle, SafeBundle(bundle).unsafe)
    }

    @Test
    fun `WHEN toSafeBundle wraps an bundle THEN it has the same unsafe bundle as the SafeBundle constructor`() {
        // SafeBundle does not override .equals so we have to do comparison with their underlying unsafe bundles.
        assertEquals(SafeBundle(bundle).unsafe, bundle.toSafeBundle().unsafe)
    }
}
