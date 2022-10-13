/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.R

/**
 * An adapter for displaying the permissions of an add-on.
 *
 * @property permissions The list of [mozilla.components.feature.addons.Addon] permissions to display.
 * @property style Indicates how permission items should look like.
 */
class AddonPermissionsAdapter(
    private val permissions: List<String>,
    private val style: Style? = null,
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): PermissionViewHolder {
        val context = parent.context
        val inflater = LayoutInflater.from(context)
        val view = inflater.inflate(R.layout.mozac_feature_addons_permission_item, parent, false)
        val titleView = view.findViewById<TextView>(R.id.permission)
        return PermissionViewHolder(
            view,
            titleView,
        )
    }

    override fun getItemCount() = permissions.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        holder as PermissionViewHolder
        val permission = permissions[position]
        with(holder.textView) {
            text = permission
            style?.maybeSetItemTextColor(this)
        }
    }

    /**
     * A view holder for displaying the permissions of an add-on.
     */
    class PermissionViewHolder(
        val view: View,
        val textView: TextView,
    ) : RecyclerView.ViewHolder(view)

    /**
     * Allows to customize how permission items should look like.
     */
    data class Style(@ColorRes val itemsTextColor: Int? = null) {
        internal fun maybeSetItemTextColor(textView: TextView) {
            itemsTextColor?.let {
                val color = ContextCompat.getColor(textView.context, it)
                textView.setTextColor(color)
            }
        }
    }
}
