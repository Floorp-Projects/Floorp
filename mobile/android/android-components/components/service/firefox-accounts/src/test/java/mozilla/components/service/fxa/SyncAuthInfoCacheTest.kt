/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SyncAuthInfoCacheTest {
    @Test
    fun `public api`() {
        val cache = SyncAuthInfoCache(testContext)
        assertNull(cache.getCached())

        val authInfo = SyncAuthInfo(
            kid = "testKid",
            fxaAccessToken = "fxaAccess",
            // expires in the future (in seconds)
            fxaAccessTokenExpiresAt = (System.currentTimeMillis() / 1000L) + 60,
            syncKey = "long secret key",
            tokenServerUrl = "http://www.token.server/url",
        )

        cache.setToCache(authInfo)

        assertEquals(authInfo, cache.getCached())
        assertFalse(cache.expired())

        val authInfo2 = authInfo.copy(
            // expires in the past (in seconds)
            fxaAccessTokenExpiresAt = (System.currentTimeMillis() / 1000L) - 60,
        )

        cache.setToCache(authInfo2)
        assertEquals(authInfo2, cache.getCached())
        assertTrue(cache.expired())

        cache.clear()

        assertNull(cache.getCached())
    }
}
