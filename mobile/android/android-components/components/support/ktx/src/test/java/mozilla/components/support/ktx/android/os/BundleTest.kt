/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import androidx.core.os.bundleOf
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.Breadcrumb
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Date

@RunWith(AndroidJUnit4::class)
class BundleTest {

    @Test
    fun `bundles with different sizes should not be equals`() {
        val small = bundleOf(
            "hello" to "world",
        )
        val big = bundleOf(
            "hello" to "world",
            "foo" to "bar",
        )
        assertFalse(small.contentEquals(big))
    }

    @Test
    fun `bundles with arrays should be equal`() {
        val (bundle1, bundle2) = (0..1).map {
            bundleOf(
                "str" to "world",
                "int" to 0,
                "boolArray" to booleanArrayOf(true, false),
                "byteArray" to "test".toByteArray(),
                "charArray" to "test".toCharArray(),
                "doubleArray" to doubleArrayOf(0.0, 1.1),
                "floatArray" to floatArrayOf(1f, 2f),
                "intArray" to intArrayOf(0, 1, 2),
                "longArray" to longArrayOf(0L, 1L),
                "shortArray" to shortArrayOf(1, 2),
                "typedArray" to arrayOf("foo", "bar"),
                "nestedBundle" to bundleOf(),
            )
        }
        assertTrue(bundle1.contentEquals(bundle2))
    }

    @Test
    fun `bundles with parcelables should be equal`() {
        val date = Date()
        val (bundle1, bundle2) = (0..1).map {
            bundleOf(
                "crumbs" to Breadcrumb(
                    message = "msg",
                    level = Breadcrumb.Level.DEBUG,
                    date = date,
                ),
            )
        }
        assertTrue(bundle1.contentEquals(bundle2))
    }
}
