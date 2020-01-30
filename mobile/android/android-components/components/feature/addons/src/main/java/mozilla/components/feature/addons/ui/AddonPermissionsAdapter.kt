/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.R

/**
 * An adapter for displaying the permissions of an add-on.
 *
 * @property permissions The list of [Addon] permissions to display.
 */
class AddonPermissionsAdapter(
    private val permissions: List<String>
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): PermissionViewHolder {
        val context = parent.context
        val inflater = LayoutInflater.from(context)
        val view = inflater.inflate(R.layout.mozac_feature_addons_permission_item, parent, false)
        val titleView = view.findViewById<TextView>(R.id.permission)
        return PermissionViewHolder(
            view,
            titleView
        )
    }

    override fun getItemCount() = permissions.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        holder as PermissionViewHolder
        val permission = permissions[position]
        holder.textView.text = permission
    }

    /**
     * A view holder for displaying the permissions of an add-on.
     */
    class PermissionViewHolder(
        val view: View,
        val textView: TextView
    ) : RecyclerView.ViewHolder(view)
}
