/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.R

private object LoginItemDiffCallback : DiffUtil.ItemCallback<Login>() {
    override fun areItemsTheSame(oldItem: Login, newItem: Login) =
        oldItem.guid == newItem.guid

    override fun areContentsTheSame(oldItem: Login, newItem: Login) =
        oldItem == newItem
}

/**
 * RecyclerView adapter for displaying login items.
 */
internal class BasicLoginAdapter(
    private val onLoginSelected: (Login) -> Unit,
) : ListAdapter<Login, LoginViewHolder>(LoginItemDiffCallback) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): LoginViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.login_selection_list_item, parent, false)
        return LoginViewHolder(view, onLoginSelected)
    }

    override fun onBindViewHolder(holder: LoginViewHolder, position: Int) {
        holder.bind(getItem(position))
    }
}

/**
 * View holder for a login item.
 */
internal class LoginViewHolder(
    itemView: View,
    private val onLoginSelected: (Login) -> Unit,
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    @VisibleForTesting
    lateinit var login: Login

    init {
        itemView.setOnClickListener(this)
    }

    fun bind(login: Login) {
        this.login = login
        itemView.findViewById<TextView>(R.id.username)?.text = login.username
        itemView.findViewById<TextView>(R.id.password)?.text = login.password
    }

    override fun onClick(v: View?) {
        onLoginSelected(login)
    }
}
