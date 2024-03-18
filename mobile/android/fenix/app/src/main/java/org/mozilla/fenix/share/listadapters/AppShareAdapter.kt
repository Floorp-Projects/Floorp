/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mozilla.fenix.share.ShareToAppsInteractor
import org.mozilla.fenix.share.viewholders.AppViewHolder

/**
 * Adapter for a list of apps that can be shared to.
 */
class AppShareAdapter(
    private val interactor: ShareToAppsInteractor,
) : ListAdapter<AppShareOption, AppViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AppViewHolder.LAYOUT_ID, parent, false)

        return AppViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: AppViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<AppShareOption>() {
        override fun areItemsTheSame(oldItem: AppShareOption, newItem: AppShareOption) =
            oldItem.activityName == newItem.activityName

        override fun areContentsTheSame(oldItem: AppShareOption, newItem: AppShareOption) =
            oldItem == newItem
    }
}

/**
 * Represents an app that can be shared to.
 *
 * @property name Name of the app.
 * @property icon Icon representing the share target.
 * @property packageName Package of the app.
 * @property activityName Activity that will be shared to.
 */
data class AppShareOption(
    val name: String,
    val icon: Drawable,
    val packageName: String,
    val activityName: String,
)
