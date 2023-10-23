/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
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
                "intl.accept_languages" to "fr-CA, it, de",
            ),
        )
    }

    private var expectedLanguages: List<TranslationsController.Language> = listOf(
        Language("bg", "Bulgarian"),
        Language("nl", "Dutch"),
        Language("en", "English"),
        Language("fr", "French"),
        Language("de", "German"),
        Language("it", "Italian"),
        Language("pl", "Polish"),
        Language("pt", "Portuguese"),
        Language("es", "Spanish"),
    )

    @Test
    fun onExpectedTranslateDelegateTest() {
        sessionRule.delegateDuringNextWait(object : Delegate {
            @AssertCalled(count = 0)
            override fun onExpectedTranslate(session: GeckoSession) {
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
        // ToDo: bug 1853469 is to research getting automation testing working fully.
    }

    @Test
    fun onOfferTranslateDelegateTest() {
        sessionRule.delegateDuringNextWait(object : Delegate {
            @AssertCalled(count = 0)
            override fun onOfferTranslate(session: GeckoSession) {
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
        // ToDo: bug 1853469 is to research getting onOfferTranslate to work in automation.
    }

    @Test
    fun onTranslationStateChangeDelegateTest() {
        sessionRule.delegateDuringNextWait(object : Delegate {
            @AssertCalled(count = 1)
            override fun onTranslationStateChange(
                session: GeckoSession,
                translationState: TranslationState?,
            ) {
                assertTrue(
                    "Translations correctly does not have an engine ready.",
                    translationState?.isEngineReady == false,
                )
                assertTrue("Translations correctly does not have a requested pair. ", translationState?.requestedTranslationPair == null)
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
    }

    @Test
    fun translateTest() {
        sessionRule.delegateUntilTestEnd(object : Delegate {
            @AssertCalled(count = 1)
            override fun onTranslationStateChange(
                session: GeckoSession,
                translationState: TranslationState?,
            ) {
                assertTrue(
                    "Translations correctly does not have an engine ready. ",
                    translationState?.isEngineReady == false,
                )
                assertTrue("Translations correctly does not have a requested pair. ", translationState?.requestedTranslationPair == null)
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()
        val translate = mainSession.sessionTranslation!!.translate("es", "en", null)
        try {
            sessionRule.waitForResult(translate)
            // ToDo: bug 1853469 models not available in automation
            assertTrue("Should not be able to translate.", false)
        } catch (e: Exception) {
            assertTrue("Should have an exception.", true)
        }
    }

    @Test
    fun restoreOriginalPageLanguageTest() {
        sessionRule.delegateUntilTestEnd(object : Delegate {
            @AssertCalled(count = 2)
            override fun onTranslationStateChange(
                session: GeckoSession,
                translationState: TranslationState?,
            ) {
                assertTrue(
                    "Translations correctly does not have an engine ready. ",
                    translationState?.isEngineReady == false,
                )
                assertTrue("Translations correctly does not have a requested pair. ", translationState?.requestedTranslationPair == null)
            }
        })
        mainSession.loadTestPath(TRANSLATIONS_ES)
        mainSession.waitForPageStop()

        val restore = mainSession.sessionTranslation!!.restoreOriginalPage()
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
        // ToDo: bug 1853055 will develop this further.
    }

    @Test
    fun testIsTranslationsEngineSupported() {
        sessionRule.setPrefsUntilTestEnd(mapOf("browser.translations.simulateUnsupportedEngine" to false))
        val isSupportedResult = TranslationsController.RuntimeTranslation.isTranslationsEngineSupported()
        assertTrue(
            "The translations engine is correctly reporting as supported.",
            sessionRule.waitForResult(isSupportedResult),
        )
        // ToDo: Bug 1853469
        // Setting "browser.translations.simulateUnsupportedEngine" to true did not return false.
    }

    @Test
    fun testGetPreferredLanguage() {
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
        // Will revisit during or after Bug 1853469, might also not be advisable to test downloading/deleting full models in automation.
        val options = ModelManagementOptions.Builder()
            .languageToManage("en")
            .operation(TranslationsController.RuntimeTranslation.DOWNLOAD)
            .build()

        assertTrue("ModelManagementOptions builder options work as expected.", options.language == "en" && options.operation == DOWNLOAD)
    }

    @Test
    fun testListSupportedLanguages() {
        val translationDropdowns = TranslationsController.RuntimeTranslation.listSupportedLanguages()
        // Bug 1853469 models are not listing in automation, so we do not have information on what models are enabled.
        try {
            sessionRule.waitForResult(translationDropdowns)
            assertTrue("Should not be able to list supported languages.", false)
        } catch (e: Exception) {
            assertTrue("Should have an exception.", true)
        }

//        Enable after ToDo: Bug 1853469
//        var fromPresent = true
//        var toPresent = true
//        sessionRule.waitForResult(translationDropdowns).let { dropdowns ->
//            // It is okay and expected that additional languages will join these lists over time.
//            // Test is checking for minimum options are present.
//            for (expected in expectedLanguages) {
//                if (!dropdowns.fromLanguages!!.contains(expected)) {
//                    assertTrue("Language $expected.localizedDisplayName was not in from list", false)
//                    fromPresent = false
//                }
//                if (!dropdowns.toLanguages!!.contains(expected)) {
//                    assertTrue("Language $expected.localizedDisplayName was not in to list", false)
//                    toPresent = false
//                }
//            }
//        }
//        assertTrue(
//            "All primary from languages are present.",
//            fromPresent,
//        )
//        assertTrue(
//            "All primary to languages are present.",
//            toPresent,
//        )
    }

    @Test
    fun testListModelDownloadStates() {
        var modelStatesResult = TranslationsController.RuntimeTranslation.listModelDownloadStates()
        // ToDo: Bug 1853469 models not listing in automation, so we do not have information on the state.
        try {
            sessionRule.waitForResult(modelStatesResult)
            assertTrue("Should not be able to list models.", false)
        } catch (e: Exception) {
            assertTrue("Should have an exception.", true)
        }

//        Enable after ToDo: Bug 1853469
//        sessionRule.waitForResult(modelStatesResult).let { models ->
//            assertTrue(
//                "Received information on the state of the models.",
//                models.size >= expectedLanguages.size,
//            )
//            assertTrue(
//                "Received information on the size in bytes of the first returned model.",
//                models[0].size > 0,
//            )
//            assertTrue(
//                "Received information on the language of the first returned model.",
//                models[0].language != null,
//            )
//            assertTrue(
//                "Received information on the download state of the first returned model",
//                models[0].isDownloaded,
//            )
//        }
    }
}
