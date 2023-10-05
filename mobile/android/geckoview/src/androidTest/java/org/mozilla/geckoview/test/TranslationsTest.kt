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
            ),
        )
    }

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
}
