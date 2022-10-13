/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.browser.customtabs.CustomTabsSessionToken
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.customtabs.store.OriginRelationPair
import mozilla.components.feature.customtabs.store.ValidateRelationshipAction
import mozilla.components.feature.customtabs.store.VerificationStatus.FAILURE
import mozilla.components.feature.customtabs.store.VerificationStatus.PENDING
import mozilla.components.feature.customtabs.store.VerificationStatus.SUCCESS
import mozilla.components.feature.customtabs.verify.OriginVerifier
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class OriginVerifierFeatureTest {

    @Test
    fun `verify fails if no creatorPackageName is saved`() = runTest {
        val feature = OriginVerifierFeature(mock(), mock(), mock())

        assertFalse(feature.verify(CustomTabState(), mock(), RELATION_HANDLE_ALL_URLS, mock()))
    }

    @Test
    fun `verify returns existing relationship`() = runTest {
        val feature = OriginVerifierFeature(mock(), mock(), mock())
        val origin = "https://example.com".toUri()
        val state = CustomTabState(
            creatorPackageName = "com.example.twa",
            relationships = mapOf(
                OriginRelationPair(origin, RELATION_HANDLE_ALL_URLS) to SUCCESS,
                OriginRelationPair(origin, RELATION_USE_AS_ORIGIN) to FAILURE,
                OriginRelationPair("https://sample.com".toUri(), RELATION_HANDLE_ALL_URLS) to PENDING,
            ),
        )

        assertTrue(feature.verify(state, mock(), RELATION_HANDLE_ALL_URLS, origin))
        assertFalse(feature.verify(state, mock(), RELATION_USE_AS_ORIGIN, origin))
    }

    @Test
    fun `verify checks new relationships`() = runTest {
        val store: CustomTabsServiceStore = mock()
        val verifier: OriginVerifier = mock()
        val feature = spy(OriginVerifierFeature(mock(), mock()) { store.dispatch(it) })
        doReturn(verifier).`when`(feature).getVerifier(anyString(), anyInt())
        doReturn(true).`when`(verifier).verifyOrigin(any())

        val token: CustomTabsSessionToken = mock()
        val origin = "https://sample.com".toUri()
        val state = CustomTabState(creatorPackageName = "com.example.twa")
        assertNotNull(state)

        assertTrue(feature.verify(state, token, RELATION_HANDLE_ALL_URLS, origin))

        verify(verifier).verifyOrigin(origin)
        verify(store).dispatch(ValidateRelationshipAction(token, RELATION_HANDLE_ALL_URLS, origin, PENDING))
        verify(store).dispatch(ValidateRelationshipAction(token, RELATION_HANDLE_ALL_URLS, origin, SUCCESS))
    }
}
