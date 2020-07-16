/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.view

import android.text.InputType
import android.view.ViewGroup
import androidx.core.content.res.ResourcesCompat
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_login_detail.*
import kotlinx.android.synthetic.main.fragment_login_detail.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.logins.LoginsListState

/**
 * View that contains and configures the Login Details
 */
@Suppress("ForbiddenComment")
class LoginDetailView(override val containerView: ViewGroup) : LayoutContainer {
    private val context = containerView.context

    fun update(login: LoginsListState) {
        webAddressText.text = login.currentItem?.origin
        usernameText.text = login.currentItem?.username
        passwordText.text = login.currentItem?.password
    }

    fun togglePasswordReveal(show: Boolean) {
        if (show) showPassword() else { hidePassword() }
    }

    // TODO: create helper class for toggling passwords. https://github.com/mozilla-mobile/fenix/issues/12554
    fun showPassword() {
        if (passwordText.inputType == InputType.TYPE_TEXT_VARIATION_PASSWORD or InputType.TYPE_CLASS_TEXT) {
            context?.components?.analytics?.metrics?.track(Event.ViewLoginPassword)
            passwordText.inputType = InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            revealPasswordButton.setImageDrawable(
                ResourcesCompat.getDrawable(
                    context.resources,
                    R.drawable.mozac_ic_password_hide, null
                )
            )
            revealPasswordButton.contentDescription =
                context.resources.getString(R.string.saved_login_hide_password)
        }
        // For the new type to take effect you need to reset the text
        passwordText.text = containerView.passwordText.editableText
    }

    fun hidePassword() {
        passwordText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        revealPasswordButton.setImageDrawable(
            ResourcesCompat.getDrawable(context.resources,
                R.drawable.mozac_ic_password_reveal, null)
        )
        revealPasswordButton.contentDescription =
            context.getString(R.string.saved_login_reveal_password)
        // For the new type to take effect you need to reset the text
        passwordText.text = containerView.passwordText.editableText
    }
}
