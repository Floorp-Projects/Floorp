/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.CreditCard
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.consumePromptFrom
import mozilla.components.support.base.log.logger.Logger

/**
 * Interactor that implements [SelectablePromptView.Listener] and notifies the feature about actions
 * the user performed in the credit card picker.
 *
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property creditCardSelectBar The [SelectablePromptView] view into which the select credit card
 * prompt will be inflated.
 * @property manageCreditCardsCallback A callback invoked when a user selects "Manage credit cards"
 * from the select credit card prompt.
 * @property sessionId The session ID which requested the prompt.
 */
class CreditCardPicker(
    private val store: BrowserStore,
    private val creditCardSelectBar: SelectablePromptView<CreditCard>,
    private val manageCreditCardsCallback: () -> Unit = {},
    private var sessionId: String? = null
) : SelectablePromptView.Listener<CreditCard> {

    init {
        creditCardSelectBar.listener = this
    }

    override fun onManageOptions() {
        manageCreditCardsCallback.invoke()
        dismissSelectCreditCardRequest()
    }

    override fun onOptionSelect(option: CreditCard) {
        store.consumePromptFrom(sessionId) {
            if (it is PromptRequest.SelectCreditCard) it.onConfirm(option)
        }

        creditCardSelectBar.hidePrompt()
    }

    /**
     * Dismisses the active select credit card request.
     *
     * @param promptRequest The current active [PromptRequest.SelectCreditCard] or null
     * otherwise.
     */
    @Suppress("TooGenericExceptionCaught")
    fun dismissSelectCreditCardRequest(promptRequest: PromptRequest.SelectCreditCard? = null) {
        creditCardSelectBar.hidePrompt()

        try {
            if (promptRequest != null) {
                promptRequest.onDismiss()
                return
            }

            store.consumePromptFrom(sessionId) {
                if (it is PromptRequest.SelectCreditCard) it.onDismiss()
            }
        } catch (e: RuntimeException) {
            Logger.error("Can't dismiss this select credit card prompt", e)
        }
    }

    /**
     * Shows the select credit card prompt in response to the [PromptRequest] event.
     *
     * @param request The [PromptRequest] containing the the credit card request data to be shown.
     */
    internal fun handleSelectCreditCardRequest(request: PromptRequest.SelectCreditCard) {
        creditCardSelectBar.showPrompt(request.creditCards)
    }
}
