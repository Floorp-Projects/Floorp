/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertTrue
import org.json.JSONObject
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.TranslationsController
import org.mozilla.geckoview.TranslationsController.Language
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.DOWNLOAD
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.ModelManagementOptions
import org.mozilla.geckoview.TranslationsController.SessionTranslation.Delegate
import org.mozilla.geckoview.TranslationsController.SessionTranslation.TranslationOptions
import org.mozilla.geckoview.TranslationsController.SessionTranslation.TranslationState
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class TranslationsTest : BaseSessionTest() {
    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "browser.translations.enable" to true,
                "browser.translations.automaticallyPopup" to true,
                "intl.accept_languages" to "en",
                "browser.translations.geckoview.enableAllTestMocks" to true,
            ),
        )
    }

    @After
    fun cleanup() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "browser.translations.automaticallyPopup" to false,
                "browser.translations.geckoview.enableAllTestMocks" to false,
            ),
        )
    }

    private var mockedExpectedLanguages: List<TranslationsController.Language> = listOf(
        Language("en", "English"),
        Language("es", "Spanish"),
    )

    @Test
    fun onExpectedTranslateDelegateTest() {
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()

        val handled = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : Delegate {
            @AssertCalled(count = 1)
            override fun onExpectedTranslate(session: GeckoSession) {
                handled.complete(null)
            }
        })
        var expectedTranslateEvent = JSONObject(
            """
            {
            "detectedLanguages": {
              "userLangTag": "en",
              "isDocLangTagSupported": true,
              "docLangTag": "es"
            },
            "requestedTranslationPair": null,
            "error": null,
            "isEngineReady": false
            }
            """.trimIndent(),
        )
        mainSession.triggerLanguageStateChange(expectedTranslateEvent)
        sessionRule.waitForResult(handled)
    }

    @Test
    fun onOfferTranslateDelegateTest() {
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()

        val handled = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : Delegate {
            @AssertCalled(count = 1)
            override fun onOfferTranslate(session: GeckoSession) {
                handled.complete(null)
            }
        })

        mainSession.triggerTranslationsOffer()
        sessionRule.waitForResult(handled)
    }

    @Test
    fun onTranslationStateChangeDelegateTest() {
        sessionRule.delegateDuringNextWait(object : Delegate {
            @AssertCalled(count = 1)
            override fun onTranslationStateChange(
                session: GeckoSession,
                translationState: TranslationState?,
            ) {
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
    }

    // Simpler translation test that doesn't test delegate state.
    // Tests en -> es
    @Test
    fun simpleTranslateTest() {
        mainSession.loadTestPath(TRANSLATIONS_EN)
        mainSession.waitForPageStop()
        val translate = sessionRule.session.sessionTranslation!!.translate("en", "es", null)
        try {
            sessionRule.waitForResult(translate)
            assertTrue("Translate should complete", true)
        } catch (e: Exception) {
            assertTrue("Should not have an exception while translating.", false)
        }
    }

    // More comprehensive translation test that also tests delegate state.
    // Tests es -> en
    @Test
    fun translateTest() {
        var delegateCalled = 0
        sessionRule.delegateUntilTestEnd(object : Delegate {
            @AssertCalled(count = 3)
            override fun onTranslationStateChange(
                session: GeckoSession,
                translationState: TranslationState?,
            ) {
                delegateCalled++
                // Before page load
                if (delegateCalled == 1) {
                    assertTrue(
                        "Translations correctly does not have a requested pair.",
                        translationState?.requestedTranslationPair == null,
                    )
                }
                // Page load
                if (delegateCalled == 2) {
                    assertTrue("Translations correctly has detected a page language. ", translationState?.detectedLanguages?.docLangTag == "es")
                }

                // Translate
                if (delegateCalled == 3) {
                    assertTrue("Translations correctly has set a translation pair from language. ", translationState?.requestedTranslationPair?.fromLanguage == "es")
                    assertTrue("Translations correctly has set a translation pair to language. ", translationState?.requestedTranslationPair?.toLanguage == "en")
                }
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
        val translate = sessionRule.session.sessionTranslation!!.translate("es", "en", null)
        try {
            sessionRule.waitForResult(translate)
            assertTrue("Should be able to translate.", true)
        } catch (e: Exception) {
            assertTrue("Should not have an exception.", false)
        }
    }

    @Test
    fun restoreOriginalPageLanguageTest() {
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
        val restore = sessionRule.session.sessionTranslation!!.restoreOriginalPage()
        try {
            sessionRule.waitForResult(restore)
            assertTrue("Should be able to restore.", true)
        } catch (e: Exception) {
            assertTrue("Should not have an exception.", false)
        }
    }

    @Test
    fun testTranslationOptions() {
        var options = TranslationOptions.Builder().downloadModel(true).build()
        assertTrue("TranslationOptions builder options work as expected.", options.downloadModel)
    }

    @Test
    fun testIsTranslationsEngineSupported() {
        sessionRule.setPrefsUntilTestEnd(mapOf("browser.translations.simulateUnsupportedEngine" to false))
        val isSupportedResult = TranslationsController.RuntimeTranslation.isTranslationsEngineSupported()
        assertTrue(
            "The translations engine is correctly reporting as supported.",
            sessionRule.waitForResult(isSupportedResult),
        )
    }

    @Test
    fun testIsTranslationsEngineNotSupported() {
        sessionRule.setPrefsUntilTestEnd(mapOf("browser.translations.simulateUnsupportedEngine" to true))
        val isSupportedResult = TranslationsController.RuntimeTranslation.isTranslationsEngineSupported()
        assertTrue(
            "The translations engine is correctly reporting as not supported.",
            sessionRule.waitForResult(isSupportedResult) == false,
        )
    }

    @Test
    fun testGetPreferredLanguage() {
        sessionRule.setPrefsUntilTestEnd(mapOf("intl.accept_languages" to "fr-CA, it, de"))
        val preferredLanguages = TranslationsController.RuntimeTranslation.preferredLanguages()
        sessionRule.waitForResult(preferredLanguages).let { languages ->
            assertTrue(
                "French is the first language preference.",
                languages[0] == "fr",
            )
            assertTrue(
                "Italian is the second language preference.",
                languages[1] == "it",
            )
            assertTrue(
                "German is the third language preference.",
                languages[2] == "de",
            )
            // "en" is likely the 4th preference via system language;
            // however, this is difficult to guarantee/set in automation.
        }
    }

    @Test
    fun testManageLanguageModel() {
        val options = ModelManagementOptions.Builder()
            .languageToManage("en")
            .operation(TranslationsController.RuntimeTranslation.DOWNLOAD)
            .build()

        assertTrue("ModelManagementOptions builder options work as expected.", options.language == "en" && options.operation == DOWNLOAD)
    }

    @Test
    fun testListSupportedLanguages() {
        val translationDropdowns = TranslationsController.RuntimeTranslation.listSupportedLanguages()
        try {
            sessionRule.waitForResult(translationDropdowns)
            assertTrue("Should be able to list supported languages.", true)
        } catch (e: Exception) {
            assertTrue("Should not have an exception.", false)
        }
        var fromPresent = true
        var toPresent = true
        sessionRule.waitForResult(translationDropdowns).let { dropdowns ->
            // Test is checking for minimum options are present based on mocked expectations.
            for (expected in mockedExpectedLanguages) {
                if (!dropdowns.fromLanguages!!.contains(expected)) {
                    assertTrue("Language $expected was not in from list.", false)
                    fromPresent = false
                }
                if (!dropdowns.toLanguages!!.contains(expected)) {
                    assertTrue("Language $expected was not in to list.", false)
                    toPresent = false
                }
            }
        }
        assertTrue(
            "All primary from languages are present.",
            fromPresent,
        )
        assertTrue(
            "All primary to languages are present.",
            toPresent,
        )
    }

    @Test
    fun testListModelDownloadStates() {
        var modelStatesResult = TranslationsController.RuntimeTranslation.listModelDownloadStates()
        try {
            sessionRule.waitForResult(modelStatesResult)
            assertTrue("Should not be able to list models.", true)
        } catch (e: Exception) {
            assertTrue("Should not have an exception.", false)
        }

        sessionRule.waitForResult(modelStatesResult).let { models ->
            assertTrue(
                "Received information on the state of the models.",
                models.size >= mockedExpectedLanguages.size - 1,
            )
            assertTrue(
                "Received information on the size in bytes of the first returned model.",
                models[0].size > 0,
            )
            assertTrue(
                "Received information on the language of the first returned model.",
                models[0].language != null,
            )
            assertTrue(
                "Received information on the download state of the first returned model",
                !models[0].isDownloaded,
            )
        }
    }
}
