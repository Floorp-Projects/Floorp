/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.viewholders

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.view.isInvisible
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.app_share_list_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.share.ShareToAppsInteractor
import org.mozilla.fenix.share.listadapters.AndroidShareOption

class AppViewHolder(
    itemView: View,
    @VisibleForTesting val interactor: ShareToAppsInteractor
) : RecyclerView.ViewHolder(itemView) {

    private var application: AndroidShareOption? = null

    init {
        itemView.setOnClickListener {
            Do exhaustive when (val app = application) {
                AndroidShareOption.Invisible, null -> { /* no-op */ }
                is AndroidShareOption.App -> interactor.onShareToApp(app)
            }
        }
    }

    fun bind(item: AndroidShareOption) {
        application = item

        when (item) {
            AndroidShareOption.Invisible -> {
                itemView.isInvisible = true
            }
            is AndroidShareOption.App -> {
                itemView.isInvisible = false
                itemView.appName.text = item.name
                itemView.appIcon.setImageDrawable(item.icon)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.app_share_list_item
    }
}
