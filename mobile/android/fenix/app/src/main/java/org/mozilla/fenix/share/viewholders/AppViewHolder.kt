/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.viewholders

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.AppShareListItemBinding
import org.mozilla.fenix.share.ShareToAppsInteractor
import org.mozilla.fenix.share.listadapters.AppShareOption

class AppViewHolder(
    itemView: View,
    @get:VisibleForTesting val interactor: ShareToAppsInteractor,
) : RecyclerView.ViewHolder(itemView) {

    private var application: AppShareOption? = null

    init {
        itemView.setOnClickListener {
            application?.let { app ->
                interactor.onShareToApp(app)
            }
        }
    }

    fun bind(item: AppShareOption) {
        application = item
        val binding = AppShareListItemBinding.bind(itemView)
        binding.appName.text = item.name
        binding.appIcon.setImageDrawable(item.icon)
    }

    companion object {
        const val LAYOUT_ID = R.layout.app_share_list_item
    }
}
