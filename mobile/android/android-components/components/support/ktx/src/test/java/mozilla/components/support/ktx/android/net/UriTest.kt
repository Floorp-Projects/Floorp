/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.net

import mozilla.components.support.ktx.kotlin.toUri
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class UriTest {
    @Test
    fun hostWithoutCommonPrefixes() {
        assertEquals(
            "mozilla.org",
            "https://www.mozilla.org".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "twitter.com",
            "https://mobile.twitter.com/home".toUri().hostWithoutCommonPrefixes)

        assertNull("".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "",
            "http://".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "facebook.com",
            "https://m.facebook.com/".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "github.com",
            "https://github.com/mozilla-mobile/android-components".toUri().hostWithoutCommonPrefixes)
    }
}
