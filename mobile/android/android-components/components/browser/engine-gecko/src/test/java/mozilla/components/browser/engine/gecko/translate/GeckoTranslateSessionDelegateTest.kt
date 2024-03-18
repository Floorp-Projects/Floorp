/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.translate
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertTrue
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.TranslationsController

@RunWith(AndroidJUnit4::class)
class GeckoTranslateSessionDelegateTest {
    private lateinit var runtime: GeckoRuntime
    private lateinit var mockSession: GeckoEngineSession

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
        mockSession = GeckoEngineSession(runtime)
    }

    @Test
    fun `WHEN onExpectedTranslate is called THEN notify onTranslateExpected`() {
        var onTranslateExpectedWasCalled = false
        val gecko = GeckoTranslateSessionDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onTranslateExpected() {
                    onTranslateExpectedWasCalled = true
                }
            },
        )

        gecko.onExpectedTranslate(mock())

        assertTrue(onTranslateExpectedWasCalled)
    }

    @Test
    fun `WHEN onOfferTranslate is called THEN notify onTranslateOffer`() {
        var onTranslateOfferWasCalled = false
        val gecko = GeckoTranslateSessionDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onTranslateOffer() {
                    onTranslateOfferWasCalled = true
                }
            },
        )

        gecko.onOfferTranslate(mock())

        assertTrue(onTranslateOfferWasCalled)
    }

    @Test
    fun `WHEN onTranslationStateChange is called THEN notify onTranslateStateChange AND ensure mapped values are correct`() {
        var onTranslateStateChangeWasCalled = false
        val gecko = GeckoTranslateSessionDelegate(mockSession)

        // Mock state parameters to check Gecko to AC mapping is correctly occurring
        var userLangTag = "en"
        var isDocLangTagSupported = true
        var docLangTag = "es"
        var fromLanguage = "de"
        var toLanguage = "bg"
        var error = "Error!"
        var isEngineReady = false

        mockSession.register(
            object : EngineSession.Observer {
                override fun onTranslateStateChange(state: TranslationEngineState) {
                    onTranslateStateChangeWasCalled = true
                    assertTrue(state.detectedLanguages?.userPreferredLangTag == userLangTag)
                    assertTrue(state.detectedLanguages?.supportedDocumentLang == isDocLangTagSupported)
                    assertTrue(state.detectedLanguages?.documentLangTag == docLangTag)
                    assertTrue(state.requestedTranslationPair?.fromLanguage == fromLanguage)
                    assertTrue(state.requestedTranslationPair?.toLanguage == toLanguage)
                    assertTrue(state.error == error)
                    assertTrue(state.isEngineReady == isEngineReady)
                }
            },
        )

        // Mock states
        var mockDetectedLanguages = TranslationsController.SessionTranslation.DetectedLanguages(userLangTag, isDocLangTagSupported, docLangTag)
        var mockTranslationsPair = TranslationsController.SessionTranslation.TranslationPair(fromLanguage, toLanguage)
        var mockGeckoState = TranslationsController.SessionTranslation.TranslationState(mockTranslationsPair, error, mockDetectedLanguages, isEngineReady)
        gecko.onTranslationStateChange(mock(), mockGeckoState)

        assertTrue(onTranslateStateChangeWasCalled)
    }
}
