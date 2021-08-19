/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.graphics.Rect
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.Px
import androidx.core.view.isInvisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.MigrationResults
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.MigrationListItemBinding

internal data class MigrationItem(
    val migration: Migration,
    val status: Boolean = false
)

// These are the only items we want to show migrating in the UI.
internal val whiteList = linkedMapOf(
    Migration.Settings to R.string.settings_title,
    Migration.History to R.string.preferences_sync_history,
    Migration.Bookmarks to R.string.preferences_sync_bookmarks,
    Migration.Logins to R.string.migration_text_passwords
)

internal class MigrationStatusAdapter :
    ListAdapter<MigrationItem, MigrationStatusAdapter.ViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.migration_list_item, parent, false)

        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    /**
     * Filter the [results] to only include items in [whiteList] and update the adapter.
     */
    fun updateData(results: MigrationResults) {
        val itemList = whiteList.keys.map {
            if (results.containsKey(it)) {
                MigrationItem(it, results.getValue(it).success)
            } else {
                MigrationItem(it)
            }
        }
        submitList(itemList)
    }

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {

        private val context = view.context
        private val binding = MigrationListItemBinding.bind(view)
        private val title = binding.migrationItemName
        private val status = binding.migrationStatusImage

        fun bind(item: MigrationItem) {
            // Get the resource ID for the item.
            val migrationText = whiteList[item.migration]?.let {
                context.getString(it)
            }.orEmpty()
            title.text = migrationText
            status.isInvisible = !item.status
            status.contentDescription = context.getString(R.string.migration_icon_description)
        }
    }

    private object DiffCallback : DiffUtil.ItemCallback<MigrationItem>() {

        override fun areItemsTheSame(oldItem: MigrationItem, newItem: MigrationItem) =
            oldItem.migration.javaClass.simpleName == newItem.migration.javaClass.simpleName

        override fun areContentsTheSame(oldItem: MigrationItem, newItem: MigrationItem) =
            oldItem.migration.javaClass.simpleName == newItem.migration.javaClass.simpleName &&
                oldItem.status == newItem.status
    }
}

internal class MigrationStatusItemDecoration(
    @Px private val spacing: Int
) : RecyclerView.ItemDecoration() {

    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        val position = parent.getChildViewHolder(view).bindingAdapterPosition
        val itemCount = state.itemCount

        outRect.left = spacing
        outRect.right = spacing
        outRect.top = spacing
        outRect.bottom = if (position == itemCount - 1) spacing else 0
    }
}
