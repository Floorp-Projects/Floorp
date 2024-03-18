/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.consumePromptFrom
import mozilla.components.feature.prompts.facts.emitAddressAutofillDismissedFact
import mozilla.components.feature.prompts.facts.emitAddressAutofillShownFact
import mozilla.components.support.base.log.logger.Logger

/**
 * Interactor that implements [SelectablePromptView.Listener] and notifies the feature about actions
 * the user performed in the address picker.
 *
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property addressSelectBar The [SelectablePromptView] view into which the select address
 * prompt will be inflated.
 * @property onManageAddresses Callback invoked when user clicks on "Manage adresses" button from
 * select address prompt.
 * @property sessionId The session ID which requested the prompt.
 */
class AddressPicker(
    private val store: BrowserStore,
    private val addressSelectBar: SelectablePromptView<Address>,
    private val onManageAddresses: () -> Unit = {},
    private var sessionId: String? = null,
) : SelectablePromptView.Listener<Address> {

    init {
        addressSelectBar.listener = this
    }

    /**
     * Shows the select address prompt in response to the [PromptRequest] event.
     *
     * @param request The [PromptRequest] containing the the address request data to be shown.
     */
    internal fun handleSelectAddressRequest(request: PromptRequest.SelectAddress) {
        emitAddressAutofillShownFact()
        addressSelectBar.showPrompt(request.addresses)
    }

    /**
     * Dismisses the active [PromptRequest.SelectAddress] request.
     *
     * @param promptRequest The current active [PromptRequest.SelectAddress] or null
     * otherwise.
     */
    @Suppress("TooGenericExceptionCaught")
    fun dismissSelectAddressRequest(promptRequest: PromptRequest.SelectAddress? = null) {
        emitAddressAutofillDismissedFact()
        addressSelectBar.hidePrompt()

        try {
            if (promptRequest != null) {
                promptRequest.onDismiss()
                sessionId?.let {
                    store.dispatch(ContentAction.ConsumePromptRequestAction(it, promptRequest))
                }
                return
            }

            store.consumePromptFrom<PromptRequest.SelectAddress>(sessionId) {
                it.onDismiss()
            }
        } catch (e: RuntimeException) {
            Logger.error("Can't dismiss select address prompt", e)
        }
    }

    override fun onOptionSelect(option: Address) {
        store.consumePromptFrom<PromptRequest.SelectAddress>(sessionId) {
            it.onConfirm(option)
        }

        addressSelectBar.hidePrompt()
    }

    override fun onManageOptions() {
        onManageAddresses.invoke()
        dismissSelectAddressRequest()
    }
}
