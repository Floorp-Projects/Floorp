/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class FailureCacheTest {

    @Test
    fun `Cache should remember URLs for limited amount of time`() {
        val cache = spy(FailureCache())

        cache.withFixedTime(0) {
            assertFalse(hasFailedRecently("https://www.mozilla.org"))
            assertFalse(hasFailedRecently("https://www.firefox.com"))
        }

        cache.withFixedTime(50) {
            rememberFailure("https://www.mozilla.org")

            assertTrue(hasFailedRecently("https://www.mozilla.org"))
            assertFalse(hasFailedRecently("https://www.firefox.com"))
        }

        // 15 Minutes later
        cache.withFixedTime(50 + 1000 * 60 * 15) {
            assertTrue(hasFailedRecently("https://www.mozilla.org"))
            assertFalse(hasFailedRecently("https://www.firefox.com"))
        }

        // 40 Minutes later
        cache.withFixedTime(50 + 1000 * 60 * 40) {
            assertFalse(hasFailedRecently("https://www.mozilla.org"))
            assertFalse(hasFailedRecently("https://www.firefox.com"))
        }
    }
}

private fun FailureCache.withFixedTime(now: Long, block: FailureCache.() -> Unit) {
    doReturn(now).`when`(this).now()
    block()
}
