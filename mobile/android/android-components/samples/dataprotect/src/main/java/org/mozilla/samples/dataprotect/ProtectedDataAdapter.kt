/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.dataprotect

import android.content.SharedPreferences
import android.util.Base64
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.dataprotect.Keystore
import org.mozilla.samples.dataprotect.Constants.B64_FLAGS
import java.nio.charset.StandardCharsets

class ProtectedDataAdapter(
    private val prefs: SharedPreferences,
    private val keystore: Keystore,
    private val itemKeys: List<String>
) : RecyclerView.Adapter<ProtectedDataAdapter.Holder>() {
    var unlocked: Boolean = false
    fun toggleUnlock(): Boolean {
        unlocked = !unlocked
        return unlocked
    }

    override fun getItemCount(): Int = itemKeys.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.protecteddata_item, parent, false)

        return Holder(view)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) {
        val key = itemKeys[position]
        var value = prefs.getString(key, "")

        if (unlocked) {
            val encrypted = Base64.decode(value, B64_FLAGS)
            val plain = keystore.decryptBytes(encrypted)
            value = String(plain, StandardCharsets.UTF_8)
        }

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
