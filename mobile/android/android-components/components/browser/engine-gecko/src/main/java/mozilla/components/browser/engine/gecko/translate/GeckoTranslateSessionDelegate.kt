/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.translate

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationPair
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.TranslationsController

internal class GeckoTranslateSessionDelegate(
    private val engineSession: GeckoEngineSession,
) : TranslationsController.SessionTranslation.Delegate {

    /**
     * This delegate function is triggered when requesting a translation on the page is likely.
     *
     * The criteria is that the page is in a different language than the user's known languages and
     * that the page is translatable (a model is available).
     *
     * @param session The session that this delegate event corresponds to.
     */
    override fun onExpectedTranslate(session: GeckoSession) {
        engineSession.notifyObservers {
            onTranslateExpected()
        }
    }

    /**
     * This delegate function is triggered when the app should offer a translation.
     *
     * The criteria is that the translation is likely and it is the user's first visit to the host site.
     *
     * @param session The session that this delegate event corresponds to.
     */
    override fun onOfferTranslate(session: GeckoSession) {
        engineSession.notifyObservers {
            onTranslateOffer()
        }
    }

    /**
     * This delegate function is triggered when the state of the translation or translation options
     * for the page has changed. State changes usually occur on navigation or if a translation
     * action was requested, such as translating or restoring to the original page.
     *
     * This provides the translations engine state and information for the page.
     *
     * @param session The session that this delegate event corresponds to.
     * @param state The reported translations state. Not to be confused
     * with the browser translation state.
     */
    override fun onTranslationStateChange(
        session: GeckoSession,
        state: TranslationsController.SessionTranslation.TranslationState?,
    ) {
        val detectedLanguages = DetectedLanguages(
            state?.detectedLanguages?.docLangTag,
            state?.detectedLanguages?.isDocLangTagSupported,
            state?.detectedLanguages?.userLangTag,
        )
        val pair = TranslationPair(
            state?.requestedTranslationPair?.fromLanguage,
            state?.requestedTranslationPair?.toLanguage,
        )
        val translationsState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = state?.error,
            isEngineReady = state?.isEngineReady,
            hasVisibleChange = state?.hasVisibleChange,
            requestedTranslationPair = pair,
        )

        engineSession.notifyObservers {
            onTranslateStateChange(translationsState)
        }
    }
}
