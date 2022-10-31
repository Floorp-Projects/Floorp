/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.downloads.R

/**
 * An adapter for displaying the applications that can perform downloads.
 */
internal class DownloaderAppAdapter(
    context: Context,
    private val apps: List<DownloaderApp>,
    val onAppSelected: ((DownloaderApp) -> Unit),
) : RecyclerView.Adapter<DownloaderAppViewHolder>() {

    private val inflater = LayoutInflater.from(context)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DownloaderAppViewHolder {
        val view = inflater.inflate(R.layout.mozac_download_app_list_item, parent, false)

        val nameLabel = view.findViewById<TextView>(R.id.app_name)
        val iconImage = view.findViewById<ImageView>(R.id.app_icon)

        return DownloaderAppViewHolder(view, nameLabel, iconImage)
    }

    override fun getItemCount(): Int = apps.size

    override fun onBindViewHolder(holder: DownloaderAppViewHolder, position: Int) {
        val app = apps[position]
        val context = holder.itemView.context
        with(app) {
            holder.nameLabel.text = name
            holder.iconImage.setImageDrawable(app.resolver.loadIcon(context.packageManager))
            holder.bind(app, onAppSelected)
        }
    }
}

/**
 * View holder for a [DownloaderApp] item.
 */
internal class DownloaderAppViewHolder(
    itemView: View,
    val nameLabel: TextView,
    val iconImage: ImageView,
) : RecyclerView.ViewHolder(itemView) {
    fun bind(app: DownloaderApp, onAppSelected: ((DownloaderApp) -> Unit)) {
        itemView.app = app
        itemView.setOnClickListener {
            onAppSelected(it.app)
        }
    }

    internal var View.app: DownloaderApp
        get() = tag as DownloaderApp
        set(value) {
            tag = value
        }
}
