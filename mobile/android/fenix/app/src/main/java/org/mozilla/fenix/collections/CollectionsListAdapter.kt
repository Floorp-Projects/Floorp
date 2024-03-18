/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.CheckedTextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.updatePaddingRelative
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

/**
 * An adapter for displaying an option to create a new collection and the list of existing
 * collections.
 */
class CollectionsListAdapter(
    private val collections: Array<String>,
    private val onNewCollectionClicked: () -> Unit,
) : RecyclerView.Adapter<CollectionsListAdapter.CollectionItemViewHolder>() {

    @VisibleForTesting
    internal var checkedPosition = 1

    class CollectionItemViewHolder(val textView: CheckedTextView) :
        RecyclerView.ViewHolder(textView)

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): CollectionItemViewHolder {
        val textView = LayoutInflater.from(parent.context)
            .inflate(R.layout.collection_dialog_list_item, parent, false) as CheckedTextView
        return CollectionItemViewHolder(textView)
    }

    override fun onBindViewHolder(holder: CollectionItemViewHolder, position: Int) {
        if (position == 0) {
            val resources = holder.textView.resources
            holder.textView.updatePaddingRelative(
                start = resources.getDimensionPixelSize(R.dimen.tab_tray_new_collection_padding_start),
            )
            holder.textView.compoundDrawablePadding =
                resources.getDimensionPixelSize(R.dimen.tab_tray_new_collection_drawable_padding)
            holder.textView.putCompoundDrawablesRelativeWithIntrinsicBounds(
                start = AppCompatResources.getDrawable(
                    holder.textView.context,
                    R.drawable.ic_new,
                ),
            )
        } else {
            holder.textView.isChecked = checkedPosition == position
        }

        holder.textView.setOnClickListener {
            if (position == 0) {
                onNewCollectionClicked()
            } else if (checkedPosition != position) {
                notifyItemChanged(position)
                notifyItemChanged(checkedPosition)
                checkedPosition = position
            }
        }
        holder.textView.text = collections[position]
    }

    override fun getItemCount() = collections.size

    fun getSelectedCollection() = checkedPosition - 1
}
