/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.view

import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_login_detail.*
import org.mozilla.fenix.settings.logins.LoginsListState

/**
 * View that contains and configures the Login Details
 */
class LoginDetailView(override val containerView: ViewGroup) : LayoutContainer {
    fun update(login: LoginsListState) {
        webAddressText.text = login.currentItem?.origin
        usernameText.text = login.currentItem?.username
        passwordText.text = login.currentItem?.password
    }
}
