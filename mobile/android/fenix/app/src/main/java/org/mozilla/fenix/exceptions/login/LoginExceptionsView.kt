/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import android.view.ViewGroup
import androidx.core.view.isVisible
import mozilla.components.feature.logins.exceptions.LoginException
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsView

class LoginExceptionsView(
    container: ViewGroup,
    interactor: LoginExceptionsInteractor,
) : ExceptionsView<LoginException>(container, interactor) {

    override val exceptionsAdapter = LoginExceptionsAdapter(interactor)

    init {
        binding.exceptionsLearnMore.isVisible = false
        binding.exceptionsEmptyMessage.text =
            containerView.context.getString(
                R.string.preferences_passwords_exceptions_description_empty_2,
                containerView.context.getString(R.string.app_name),
            )
        binding.exceptionsList.apply {
            adapter = exceptionsAdapter
        }
    }
}
