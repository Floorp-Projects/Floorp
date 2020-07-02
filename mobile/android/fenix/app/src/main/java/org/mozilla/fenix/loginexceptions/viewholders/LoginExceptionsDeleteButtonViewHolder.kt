/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.delete_exceptions_button.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.loginexceptions.LoginExceptionsInteractor

class LoginExceptionsDeleteButtonViewHolder(
    view: View,
    private val interactor: LoginExceptionsInteractor
) : RecyclerView.ViewHolder(view) {
    private val deleteButton = view.removeAllExceptions

    init {
        deleteButton.setOnClickListener {
            interactor.onDeleteAll()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.delete_logins_exceptions_button
    }
}
