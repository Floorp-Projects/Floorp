/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import android.content.pm.PackageManager
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Response
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.RelationChecker
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.MockitoAnnotations.openMocks

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class OriginVerifierTest {

    private val androidAsset = AssetDescriptor.Android(
        packageName = "com.app.name",
        sha256CertFingerprint = "AA:BB:CC:10:20:30:01:02",
    )

    @Mock private lateinit var packageManager: PackageManager

    @Mock private lateinit var response: Response

    @Mock private lateinit var body: Response.Body

    @Mock private lateinit var checker: RelationChecker

    @Suppress("Deprecation")
    @Before
    fun setup() {
        openMocks(this)

        doReturn(body).`when`(response).body
        doReturn(200).`when`(response).status
        doReturn("{\"linked\":true}").`when`(body).string()
    }

    @Test
    fun `only HTTPS allowed`() = runTest {
        val verifier = buildVerifier(RELATION_HANDLE_ALL_URLS)
        assertFalse(verifier.verifyOrigin("LOL".toUri()))
        assertFalse(verifier.verifyOrigin("http://www.android.com".toUri()))
    }

    @Test
    fun verifyOrigin() = runTest {
        val verifier = buildVerifier(RELATION_USE_AS_ORIGIN)
        doReturn(true).`when`(checker).checkRelationship(
            AssetDescriptor.Web("https://www.example.com"),
            Relation.USE_AS_ORIGIN,
            androidAsset,
        )
        assertTrue(verifier.verifyOrigin("https://www.example.com".toUri()))
    }

    private fun buildVerifier(relation: Int): OriginVerifier {
        val verifier = spy(
            OriginVerifier(
                "com.app.name",
                relation,
                packageManager,
                checker,
            ),
        )
        doReturn(androidAsset).`when`(verifier).androidAsset
        return verifier
    }
}
