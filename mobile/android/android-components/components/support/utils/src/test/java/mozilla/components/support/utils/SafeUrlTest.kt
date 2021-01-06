/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Context
import android.content.res.Resources
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class SafeUrlTest {
    @Test
    fun `WHEN schemes blocklist is empty THEN stripUnsafeUrlSchemes should return the initial String`() {
        val resources = mock<Resources>()
        val context = mock<Context>()
        doReturn(resources).`when`(context).resources
        doReturn(emptyArray<String>()).`when`(resources).getStringArray(R.array.mozac_url_schemes_blocklist)

        val result = SafeUrl.stripUnsafeUrlSchemes(context, "unsafeText")

        assertEquals("unsafeText", result)
    }

    @Test
    fun `WHEN schemes blocklist contains items not found in the argument THEN stripUnsafeUrlSchemes should return the initial String`() {
        val resources = mock<Resources>()
        val context = mock<Context>()
        doReturn(resources).`when`(context).resources
        doReturn(arrayOf("alien")).`when`(resources).getStringArray(R.array.mozac_url_schemes_blocklist)

        val result = SafeUrl.stripUnsafeUrlSchemes(context, "thisIsAnOkText")

        assertEquals("thisIsAnOkText", result)
    }

    @Test
    fun `WHEN schemes blocklist contains items found in the argument THEN stripUnsafeUrlSchemes should recursively remove them from the front`() {
        val resources = mock<Resources>()
        val context = mock<Context>()
        doReturn(resources).`when`(context).resources
        doReturn(arrayOf("one", "two")).`when`(resources).getStringArray(R.array.mozac_url_schemes_blocklist)

        val result = SafeUrl.stripUnsafeUrlSchemes(context, "two" + "one" + "one" + "two" + "safeText")
        assertEquals("safeText", result)

        val result2 = SafeUrl.stripUnsafeUrlSchemes(context, "one" + "two" + "one" + "two" + "safeText" + "one")
        assertEquals("safeText" + "one", result2)
    }
}
