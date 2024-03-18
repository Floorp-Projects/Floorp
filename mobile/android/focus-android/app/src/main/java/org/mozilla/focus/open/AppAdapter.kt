/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.open

import android.content.Context
import android.content.pm.ActivityInfo
import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView

/**
 * An adapter for displaying app items in the [OpenWithFragment] dialog.
 * This will display browser apps and an item that can be used for installing Firefox,
 * if it is not already installed on the device.
 *
 * @param infoArray List of browser apps.
 * @param store Store app for installing Firefox.
 */
class AppAdapter(context: Context, infoArray: Array<ActivityInfo>, store: ActivityInfo?) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    /**
     * Class for an app installed on the device.
     */
    class App(private val context: Context, private val info: ActivityInfo) {
        val label: String = info.loadLabel(context.packageManager).toString()

        /**
         * Retrieve the current graphical icon associated with the app.
         */
        fun loadIcon(): Drawable {
            return info.loadIcon(context.packageManager)
        }

        val packageName: String
            get() = info.packageName
        val activityName: String
            get() = info.name
    }

    /**
     * Interface for setting a listener for an [App] item.
     */
    interface OnAppSelectedListener {
        /**
         * Action to be performed on an item from [AppAdapter] list being clicked.
         */
        fun onAppSelected(app: App)
    }

    private val apps: List<App>
    private val store: App?
    private var listener: OnAppSelectedListener? = null

    init {
        this.apps = infoArray.map { App(context, it) }
            .sortedWith { app1, app2 -> app1.label.compareTo(app2.label) }
        this.store = if (store != null) App(context, store) else null
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        if (viewType == AppViewHolder.LAYOUT_ID) {
            return AppViewHolder(
                inflater.inflate(AppViewHolder.LAYOUT_ID, parent, false),
            )
        } else if (viewType == InstallBannerViewHolder.LAYOUT_ID) {
            return InstallBannerViewHolder(
                inflater.inflate(InstallBannerViewHolder.LAYOUT_ID, parent, false),
            )
        }
        throw IllegalStateException("Unknown view type: $viewType")
    }

    /**
     * Sets the listener for the [AppAdapter].
     */
    fun setOnAppSelectedListener(listener: OnAppSelectedListener?) {
        this.listener = listener
    }

    override fun getItemViewType(position: Int): Int {
        return if (position < apps.size) {
            AppViewHolder.LAYOUT_ID
        } else {
            InstallBannerViewHolder.LAYOUT_ID
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val itemViewType = holder.itemViewType
        if (itemViewType == AppViewHolder.LAYOUT_ID) {
            bindApp(holder, position)
        } else if (itemViewType == InstallBannerViewHolder.LAYOUT_ID) {
            bindInstallBanner(holder)
        }
    }

    private fun bindApp(holder: RecyclerView.ViewHolder, position: Int) {
        val appViewHolder = holder as AppViewHolder
        val app = apps[position]
        appViewHolder.bind(app, listener)
    }

    private fun bindInstallBanner(holder: RecyclerView.ViewHolder) {
        val installViewHolder = holder as InstallBannerViewHolder
        store?.let { installViewHolder.bind(it) }
    }

    override fun getItemCount(): Int {
        return apps.size + if (store != null) 1 else 0
    }
}
