/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.net.Uri
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.OriginRelationPair
import mozilla.components.feature.customtabs.store.VerificationStatus.FAILURE
import mozilla.components.feature.customtabs.store.VerificationStatus.PENDING
import mozilla.components.feature.customtabs.store.VerificationStatus.SUCCESS
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CustomTabStateKtTest {

    @Test
    fun `trustedOrigins is empty when there are no relationships`() {
        val state = CustomTabState(relationships = emptyMap())
        assertEquals(emptyList<Uri>(), state.trustedOrigins)
    }

    @Test
    fun `trustedOrigins only includes the HANDLE_ALL_URLS relationship`() {
        val state = CustomTabState(
            relationships = mapOf(
                OriginRelationPair("https://firefox.com".toUri(), RELATION_HANDLE_ALL_URLS) to SUCCESS,
                OriginRelationPair("https://example.com".toUri(), RELATION_USE_AS_ORIGIN) to SUCCESS,
                OriginRelationPair("https://mozilla.org".toUri(), RELATION_HANDLE_ALL_URLS) to PENDING,
            ),
        )
        assertEquals(
            listOf("https://firefox.com".toUri(), "https://mozilla.org".toUri()),
            state.trustedOrigins,
        )
    }

    @Test
    fun `trustedOrigins only includes pending or success statuses`() {
        val state = CustomTabState(
            relationships = mapOf(
                OriginRelationPair("https://firefox.com".toUri(), RELATION_HANDLE_ALL_URLS) to SUCCESS,
                OriginRelationPair("https://example.com".toUri(), RELATION_USE_AS_ORIGIN) to FAILURE,
                OriginRelationPair("https://mozilla.org".toUri(), RELATION_HANDLE_ALL_URLS) to PENDING,
            ),
        )
        assertEquals(
            listOf("https://firefox.com".toUri(), "https://mozilla.org".toUri()),
            state.trustedOrigins,
        )
    }
}
