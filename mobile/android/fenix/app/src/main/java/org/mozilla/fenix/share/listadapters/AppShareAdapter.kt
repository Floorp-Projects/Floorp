/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.share.ShareToAppsInteractor
import org.mozilla.fenix.share.viewholders.AppViewHolder

class AppShareAdapter(
    private val interactor: ShareToAppsInteractor,
    private val applications: MutableList<AppShareOption> = mutableListOf()
) : RecyclerView.Adapter<AppViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AppViewHolder.LAYOUT_ID, parent, false)

        return AppViewHolder(view, interactor)
    }

    override fun getItemCount(): Int = applications.size

    override fun onBindViewHolder(holder: AppViewHolder, position: Int) {
        holder.bind(applications[position])
    }

    fun updateData(applications: List<AppShareOption>) {
        this.applications.clear()
        this.applications.addAll(applications)
        notifyDataSetChanged()
    }
}

data class AppShareOption(
    val name: String,
    val icon: Drawable,
    val packageName: String,
    val activityName: String
)
