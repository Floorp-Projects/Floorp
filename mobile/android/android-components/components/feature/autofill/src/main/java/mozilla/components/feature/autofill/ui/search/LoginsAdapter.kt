/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui.search

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.R

/**
 * Adapter for showing a list of logins.
 */
@SuppressLint("NotifyDataSetChanged")
internal class LoginsAdapter(
    private val onLoginSelected: (Login) -> Unit,
) : RecyclerView.Adapter<LoginViewHolder>() {
    private var logins: List<Login> = emptyList()

    fun update(logins: List<Login>) {
        this.logins = logins
        notifyDataSetChanged()
    }

    fun clear() {
        this.logins = emptyList()
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): LoginViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.mozac_feature_autofill_login, parent, false)
        return LoginViewHolder(view, onLoginSelected)
    }

    override fun onBindViewHolder(holder: LoginViewHolder, position: Int) {
        val login = logins[position]
        holder.bind(login)
    }

    override fun getItemCount(): Int {
        return logins.count()
    }
}
