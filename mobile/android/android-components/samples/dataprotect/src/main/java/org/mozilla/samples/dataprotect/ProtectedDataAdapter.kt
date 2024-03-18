/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.dataprotect

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.dataprotect.SecureAbove22Preferences

class ProtectedDataAdapter(
    private val prefs: SecureAbove22Preferences,
    private val itemKeys: List<String>,
) : RecyclerView.Adapter<ProtectedDataAdapter.Holder>() {
    override fun getItemCount(): Int = itemKeys.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.protecteddata_item, parent, false)

        return Holder(view)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) {
        val key = itemKeys[position]
        var value = prefs.getString(key)
        holder.keyView.text = key
        holder.valView.text = value
    }

    class Holder(val view: View) : RecyclerView.ViewHolder(view) {
        var keyView: TextView
        var valView: TextView

        init {
            keyView = view.findViewById(R.id.protecteddata_item_key_view)
            valView = view.findViewById(R.id.protecteddata_item_val_view)
        }
    }
}
