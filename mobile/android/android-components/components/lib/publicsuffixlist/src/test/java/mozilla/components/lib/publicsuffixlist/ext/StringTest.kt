/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class StringTest {

    private val publicSuffixList
        get() = PublicSuffixList(testContext)

    @Test
    fun `Url To Trimmed Host`() = runBlocking {
        val urlTest = "http://www.example.com:1080/docs/resource1.html"
        val new = urlTest.urlToTrimmedHost(publicSuffixList).await()
        assertEquals(new, "example")
    }
}
