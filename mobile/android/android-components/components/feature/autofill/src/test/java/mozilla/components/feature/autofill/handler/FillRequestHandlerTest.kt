/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.handler

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.facts.AutofillFacts
import mozilla.components.feature.autofill.response.fill.FillResponseBuilder
import mozilla.components.feature.autofill.response.fill.LoginFillResponseBuilder
import mozilla.components.feature.autofill.test.createMockStructure
import mozilla.components.feature.autofill.ui.AbstractAutofillConfirmActivity
import mozilla.components.feature.autofill.ui.AbstractAutofillSearchActivity
import mozilla.components.feature.autofill.ui.AbstractAutofillUnlockActivity
import mozilla.components.feature.autofill.verify.CredentialAccessVerifier
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import java.util.UUID

@ExperimentalCoroutinesApi // for createTestCase
@RunWith(RobolectricTestRunner::class)
internal class FillRequestHandlerTest {
    @Test
    fun `App - Twitter - With credentials`() {
        CollectionProcessor.withFactCollection { facts ->
            val credentials = generateRandomLoginFor("twitter.com")

            createTestCase<LoginFillResponseBuilder>(
                filename = "fixtures/app_twitter.xml",
                packageName = "com.twitter.android",
                logins = mapOf(credentials),
                assertThat = { builder ->
                    assertNotNull(builder!!)
                    assertEquals(1, builder.logins.size)
                    assertEquals(credentials.second, builder.logins[0])
                    assertEquals(false, builder.needsConfirmation)
                },
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_AUTOFILL, component)
                assertEquals(Action.SYSTEM, action)
                assertEquals(AutofillFacts.Items.AUTOFILL_REQUEST, item)
                assertEquals(2, metadata?.size)
                assertEquals(true, metadata?.get(AutofillFacts.Metadata.HAS_MATCHING_LOGINS))
                assertEquals(false, metadata?.get(AutofillFacts.Metadata.NEEDS_CONFIRMATION))
            }
        }
    }

    @Test
    fun `App - Twitter - Without credentials`() {
        CollectionProcessor.withFactCollection { facts ->
            createTestCase<LoginFillResponseBuilder>(
                filename = "fixtures/app_twitter.xml",
                packageName = "com.twitter.android",
                logins = emptyMap(),
                assertThat = { builder ->
                    assertNotNull(builder!!)
                    assertEquals(0, builder.logins.size)
                    assertEquals(false, builder.needsConfirmation)
                },
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_AUTOFILL, component)
                assertEquals(Action.SYSTEM, action)
                assertEquals(AutofillFacts.Items.AUTOFILL_REQUEST, item)
                assertEquals(2, metadata?.size)
                assertEquals(false, metadata?.get(AutofillFacts.Metadata.HAS_MATCHING_LOGINS))
                assertEquals(false, metadata?.get(AutofillFacts.Metadata.NEEDS_CONFIRMATION))
            }
        }
    }

    @Test
    fun `App - Expensify`() {
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/app_expensify.xml",
            packageName = "org.me.mobiexpensifyg",
            logins = mapOf(
                generateRandomLoginFor("expensify.com"),
            ),
            assertThat = { builder ->
                // Unfortunately we are not able to link the app and the website yet.
                assertNull(builder)
            },
        )
    }

    @Test
    fun `App - Facebook`() {
        val credentials = generateRandomLoginFor("facebook.com")
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/app_facebook.xml",
            packageName = "com.facebook.katana",
            logins = mapOf(credentials),
            assertThat = { builder ->
                assertNotNull(builder!!)
                assertEquals(1, builder.logins.size)
                assertEquals(credentials.second, builder.logins[0])
            },
        )
    }

    @Test
    fun `App - Facebook Lite`() {
        val credentials = generateRandomLoginFor("facebook.com")
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/app_facebook_lite.xml",
            packageName = "com.facebook.lite",
            logins = mapOf(credentials),
            assertThat = { builder ->
                assertNotNull(builder!!)
                assertEquals(1, builder.logins.size)
                assertEquals(credentials.second, builder.logins[0])
            },
        )
    }

    @Test
    fun `App - Messenger Lite`() {
        val credentials = generateRandomLoginFor("facebook.com")
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/app_messenger_lite.xml",
            packageName = "com.facebook.mlite",
            logins = mapOf(credentials),
            assertThat = { builder ->
                assertNotNull(builder!!)
                assertEquals(1, builder.logins.size)
                assertEquals(credentials.second, builder.logins[0])
            },
        )
    }

    @Test
    fun `Browser - Fenix Nightly - amazon-co-uk`() {
        val credentials = generateRandomLoginFor("amazon.co.uk")
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/browser_fenix_amazon.co.uk.xml",
            packageName = "org.mozilla.fenix",
            logins = mapOf(credentials),
            assertThat = { builder ->
                assertNotNull(builder!!)
                assertEquals(1, builder.logins.size)
                assertEquals(credentials.second, builder.logins[0])
            },
        )
    }

    @Test
    fun `Browser - WebView - gmail`() {
        val credentials = generateRandomLoginFor("accounts.google.com")
        createTestCase<LoginFillResponseBuilder>(
            filename = "fixtures/browser_webview_gmail.xml",
            packageName = "org.chromium.webview_shell",
            logins = mapOf(credentials),
            assertThat = { builder ->
                assertNotNull(builder!!)
                assertEquals(0, builder.logins.size)
                assertEquals(false, builder.needsConfirmation)
            },
        )
    }
}

@ExperimentalCoroutinesApi
private fun <B : FillResponseBuilder> FillRequestHandlerTest.createTestCase(
    filename: String,
    packageName: String,
    logins: Map<String, Login>,
    assertThat: (B?) -> Unit,
    canVerifyRelationship: Boolean = true,
) = runTest {
    val structure = createMockStructure(filename, packageName)

    val storage: LoginsStorage = mock()
    `when`(storage.getByBaseDomain(anyString())).thenAnswer { invocation ->
        val origin = invocation.getArgument(0) as String
        println("MockStorage: Query password for $origin")
        logins[origin]?.let { listOf(it) } ?: emptyList<Login>()
    }

    val verifier: CredentialAccessVerifier = mock()
    doReturn(canVerifyRelationship).`when`(verifier).hasCredentialRelationship(any(), any(), any())

    val configuration = AutofillConfiguration(
        storage = storage,
        publicSuffixList = PublicSuffixList(testContext),
        unlockActivity = AbstractAutofillUnlockActivity::class.java,
        confirmActivity = AbstractAutofillConfirmActivity::class.java,
        searchActivity = AbstractAutofillSearchActivity::class.java,
        applicationName = "Test",
        httpClient = mock(),
        verifier = verifier,
    )

    val handler = FillRequestHandler(
        testContext,
        configuration,
    )

    val builder = handler.handle(structure)
    @Suppress("UNCHECKED_CAST")
    assertThat(builder as? B)
}

private fun generateRandomLoginFor(origin: String): Pair<String, Login> {
    return origin to Login(
        guid = UUID.randomUUID().toString(),
        origin = origin,
        username = "user" + UUID.randomUUID().toString(),
        password = "password" + UUID.randomUUID().toString(),
    )
}
