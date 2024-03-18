/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.store

import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.browser.customtabs.CustomTabsSessionToken
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CustomTabsServiceStateReducerTest {

    @Test
    fun `reduce adds new tab to map`() {
        val token: CustomTabsSessionToken = mock()
        val initialState = CustomTabsServiceState()
        val action = SaveCreatorPackageNameAction(token, "com.example.twa")

        assertEquals(
            CustomTabsServiceState(
                tabs = mapOf(
                    token to CustomTabState(creatorPackageName = "com.example.twa"),
                ),
            ),
            CustomTabsServiceStateReducer.reduce(initialState, action),
        )
    }

    @Test
    fun `reduce replaces existing tab in map`() {
        val token: CustomTabsSessionToken = mock()
        val initialState = CustomTabsServiceState(
            tabs = mapOf(
                token to CustomTabState(creatorPackageName = "com.example.twa"),
            ),
        )
        val action = SaveCreatorPackageNameAction(token, "com.example.trusted.web.app")

        assertEquals(
            CustomTabsServiceState(
                tabs = mapOf(
                    token to CustomTabState(creatorPackageName = "com.example.trusted.web.app"),
                ),
            ),
            CustomTabsServiceStateReducer.reduce(initialState, action),
        )
    }

    @Test
    fun `reduce adds new relationship`() {
        val token: CustomTabsSessionToken = mock()
        val initialState = CustomTabsServiceState(
            tabs = mapOf(
                token to CustomTabState(creatorPackageName = "com.example.twa"),
            ),
        )
        val action = ValidateRelationshipAction(
            token,
            RELATION_HANDLE_ALL_URLS,
            "https://example.com".toUri(),
            VerificationStatus.PENDING,
        )

        assertEquals(
            CustomTabsServiceState(
                tabs = mapOf(
                    token to CustomTabState(
                        creatorPackageName = "com.example.twa",
                        relationships = mapOf(
                            Pair(
                                OriginRelationPair("https://example.com".toUri(), RELATION_HANDLE_ALL_URLS),
                                VerificationStatus.PENDING,
                            ),
                        ),
                    ),
                ),
            ),
            CustomTabsServiceStateReducer.reduce(initialState, action),
        )
    }

    @Test
    fun `reduce adds new relationship of different type`() {
        val token: CustomTabsSessionToken = mock()
        val initialState = CustomTabsServiceState(
            tabs = mapOf(
                token to CustomTabState(
                    creatorPackageName = "com.example.twa",
                    relationships = mapOf(
                        Pair(
                            OriginRelationPair("https://example.com".toUri(), RELATION_HANDLE_ALL_URLS),
                            VerificationStatus.FAILURE,
                        ),
                    ),
                ),
            ),
        )
        val action = ValidateRelationshipAction(
            token,
            RELATION_USE_AS_ORIGIN,
            "https://example.com".toUri(),
            VerificationStatus.PENDING,
        )

        assertEquals(
            CustomTabsServiceState(
                tabs = mapOf(
                    token to CustomTabState(
                        creatorPackageName = "com.example.twa",
                        relationships = mapOf(
                            Pair(
                                OriginRelationPair("https://example.com".toUri(), RELATION_HANDLE_ALL_URLS),
                                VerificationStatus.FAILURE,
                            ),
                            Pair(
                                OriginRelationPair("https://example.com".toUri(), RELATION_USE_AS_ORIGIN),
                                VerificationStatus.PENDING,
                            ),
                        ),
                    ),
                ),
            ),
            CustomTabsServiceStateReducer.reduce(initialState, action),
        )
    }

    @Test
    fun `reduce replaces existing relationship`() {
        val token: CustomTabsSessionToken = mock()
        val initialState = CustomTabsServiceState(
            tabs = mapOf(
                token to CustomTabState(
                    creatorPackageName = "com.example.twa",
                    relationships = mapOf(
                        Pair(
                            OriginRelationPair("https://example.com".toUri(), RELATION_HANDLE_ALL_URLS),
                            VerificationStatus.PENDING,
                        ),
                    ),
                ),
            ),
        )
        val action = ValidateRelationshipAction(
            token,
            RELATION_HANDLE_ALL_URLS,
            "https://example.com".toUri(),
            VerificationStatus.SUCCESS,
        )

        assertEquals(
            CustomTabsServiceState(
                tabs = mapOf(
                    token to CustomTabState(
                        creatorPackageName = "com.example.twa",
                        relationships = mapOf(
                            Pair(
                                OriginRelationPair("https://example.com".toUri(), RELATION_HANDLE_ALL_URLS),
                                VerificationStatus.SUCCESS,
                            ),
                        ),
                    ),
                ),
            ),
            CustomTabsServiceStateReducer.reduce(initialState, action),
        )
    }
}
