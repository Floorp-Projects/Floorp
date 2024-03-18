/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.concept.PasswordPromptView
import mozilla.components.feature.prompts.consumePromptFrom
import mozilla.components.support.base.log.logger.Logger

/**
 * Displays a [PasswordPromptView] for a site after receiving a [PromptRequest.SelectLoginPrompt]
 * when a user clicks into a login field and we don't have any matching logins. The user can receive
 * a suggestion for a strong password that can be used for filling in the password field.
 *
 * @property browserStore The [BrowserStore] this feature should subscribe to.
 * @property suggestStrongPasswordBar The view where the suggest strong password "prompt" will be inflated.
 * @property sessionId This is the id of the session which requested the prompt.
 */
internal class StrongPasswordPromptViewListener(
    private val browserStore: BrowserStore,
    private val suggestStrongPasswordBar: PasswordPromptView,
    private var sessionId: String? = null,
) : PasswordPromptView.Listener {

    init {
        suggestStrongPasswordBar.listener = this
    }

    internal fun handleSuggestStrongPasswordRequest(
        request: PromptRequest.SelectLoginPrompt,
        currentUrl: String,
        onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
    ) {
        request.generatedPassword?.let {
            suggestStrongPasswordBar.showPrompt(
                it,
                currentUrl,
                onSaveLoginWithStrongPassword,
            )
        }
    }

    @Suppress("TooGenericExceptionCaught")
    fun dismissCurrentSuggestStrongPassword(promptRequest: PromptRequest.SelectLoginPrompt? = null) {
        try {
            if (promptRequest != null) {
                promptRequest.onDismiss()
                sessionId?.let {
                    browserStore.dispatch(
                        ContentAction.ConsumePromptRequestAction(
                            it,
                            promptRequest,
                        ),
                    )
                }
                suggestStrongPasswordBar.hidePrompt()
                return
            }

            browserStore.consumePromptFrom<PromptRequest.SelectLoginPrompt>(sessionId) {
                it.onDismiss()
            }
        } catch (e: RuntimeException) {
            Logger.error("Can't dismiss this prompt", e)
        }
        suggestStrongPasswordBar.hidePrompt()
    }

    override fun onUseGeneratedPassword(
        generatedPassword: String,
        url: String,
        onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
    ) {
        browserStore.consumePromptFrom<PromptRequest.SelectLoginPrompt>(sessionId) {
            // Create complete login entry: https://bugzilla.mozilla.org/show_bug.cgi?id=1869575
            val createdLoginEntryWithPassword = Login(
                guid = "",
                origin = url,
                username = "",
                password = generatedPassword,
            )
            it.onConfirm(createdLoginEntryWithPassword)
        }
        onSaveLoginWithStrongPassword.invoke(url, generatedPassword)
        suggestStrongPasswordBar.hidePrompt()
    }
}
