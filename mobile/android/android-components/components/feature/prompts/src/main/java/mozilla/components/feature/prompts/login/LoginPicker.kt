/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.consumePromptFrom

/**
 * The [LoginPicker] displays a list of possible logins in a [LoginPickerView] for a site after
 * receiving a [PromptRequest.SelectLoginPrompt] when a user clicks into a login field and we have
 * matching logins. It allows the user to select which one of these logins they would like to fill,
 * or select an option to manage their logins.
 *
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property loginSelectBar The [LoginPickerView] view into which the select login "prompt" will be inflated.
 * @property manageLoginsCallback A callback invoked when a user selects "manage logins" from the
 * select login prompt.
 * @property sessionId This is the id of the session which requested the prompt.
 */
internal class LoginPicker(
    private val store: BrowserStore,
    private val loginSelectBar: LoginPickerView,
    private val manageLoginsCallback: () -> Unit = {},
    private var sessionId: String? = null
) : LoginPickerView.Listener {

    init {
        loginSelectBar.listener = this
    }

    internal fun handleSelectLoginRequest(request: PromptRequest.SelectLoginPrompt) {
        if (currentRequest != null) {
            store.consumePromptFrom(sessionId) {
                (it as PromptRequest.SelectLoginPrompt).onDismiss()
            }
        }
        currentRequest = request
        loginSelectBar.showPicker(request.logins)
    }

    @VisibleForTesting
    internal var currentRequest: PromptRequest.SelectLoginPrompt? = null

    override fun onLoginSelected(login: Login) {
        (currentRequest as PromptRequest.SelectLoginPrompt).onConfirm(login)
        currentRequest = null
        loginSelectBar.hidePicker()
    }

    override fun onManageLogins() {
        manageLoginsCallback.invoke()
        dismissCurrentLoginSelect()
    }

    fun dismissCurrentLoginSelect() {
        if (currentRequest != null) {
            store.consumePromptFrom(sessionId) {
                (it as PromptRequest.SelectLoginPrompt).onDismiss()
            }
        }
        currentRequest = null
        loginSelectBar.hidePicker()
    }
}
