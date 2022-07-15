/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListSignInBinding
import org.mozilla.fenix.library.history.HistoryAdapter

/**
 * A view representing a sign in window inside the synced history screen.
 * [HistoryAdapter] is responsible for creating and populating the view.
 *
 * @param view that is passed down to the parent's constructor.
 * @param onSignInClicked Invokes when a signIn button is pressed.
 * @param onCreateAccountClicked Invokes when a createAccount button is pressed.
 */
class SignInViewHolder(
    view: View,
    private val onSignInClicked: () -> Unit,
    private val onCreateAccountClicked: () -> Unit
) : RecyclerView.ViewHolder(view) {

    private val binding = HistoryListSignInBinding.bind(view)

    init {
        binding.signInButton.setOnClickListener {
            onSignInClicked.invoke()
        }
        binding.createAccount.setOnClickListener {
            onCreateAccountClicked.invoke()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_sign_in
    }
}
