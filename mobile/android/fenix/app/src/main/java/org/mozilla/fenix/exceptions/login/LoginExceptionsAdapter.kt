/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import androidx.recyclerview.widget.DiffUtil
import mozilla.components.feature.logins.exceptions.LoginException
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsAdapter

/**
 * Adapter for a list of sites that are exempted from saving logins,
 * along with controls to remove the exception.
 */
class LoginExceptionsAdapter(
    interactor: LoginExceptionsInteractor,
) : ExceptionsAdapter<LoginException>(interactor, DiffCallback) {

    override val deleteButtonLayoutId = R.layout.delete_logins_exceptions_button
    override val headerDescriptionResource = R.string.preferences_passwords_exceptions_description_2

    override fun wrapAdapterItem(item: LoginException) =
        LoginAdapterItem(item)

    data class LoginAdapterItem(
        override val item: LoginException,
    ) : AdapterItem.Item<LoginException>() {
        override val url get() = item.origin
    }

    internal object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            when (oldItem) {
                AdapterItem.DeleteButton, AdapterItem.Header -> oldItem === newItem
                is LoginAdapterItem -> newItem is LoginAdapterItem && oldItem.item.id == newItem.item.id
                else -> false
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }
}
