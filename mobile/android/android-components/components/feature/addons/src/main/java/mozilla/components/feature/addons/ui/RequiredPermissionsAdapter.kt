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
* An adapter for displaying optional or required permissions before installing an addon.
*
* @property permissions The list of the permissions that will be displayed.
*/
class RequiredPermissionsAdapter(private val permissions: List<String>) :
    RecyclerView.Adapter<RequiredPermissionsAdapter.ViewHolder>() {

    /**
     * Function used to specify where to display each permission.
     */
    class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val permissionRequired: TextView

        init {
            permissionRequired = itemView.findViewById(R.id.permission_required_item)
        }
    }

    override fun onCreateViewHolder(viewGroup: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(viewGroup.context)
            .inflate(R.layout.mozac_feature_addons_permissions_required_item, viewGroup, false)

        return ViewHolder(view)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, position: Int) {
        viewHolder.permissionRequired.text = permissions[position]
    }

    override fun getItemCount() = permissions.size

    /**
     * Function used in tests to verify the permissions.
     */
    fun getItemAtPosition(position: Int): String {
        return permissions[position]
    }
}
