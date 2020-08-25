/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.CheckedTextView
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.core.view.updatePaddingRelative
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R

internal class CollectionsAdapter(
    private val collections: Array<String>,
    private val onNewCollectionClicked: () -> Unit
) : RecyclerView.Adapter<CollectionsAdapter.CollectionItemViewHolder>() {

    @VisibleForTesting
    internal var checkedPosition = 1

    class CollectionItemViewHolder(val textView: CheckedTextView) :
        RecyclerView.ViewHolder(textView)

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): CollectionItemViewHolder {
        val textView = LayoutInflater.from(parent.context)
            .inflate(R.layout.collection_dialog_list_item, parent, false) as CheckedTextView
        return CollectionItemViewHolder(textView)
    }

    override fun onBindViewHolder(holder: CollectionItemViewHolder, position: Int) {
        if (position == 0) {
            val displayMetrics = holder.textView.context.resources.displayMetrics
            holder.textView.updatePaddingRelative(start = NEW_COLLECTION_PADDING_START.dpToPx(displayMetrics))
            holder.textView.compoundDrawablePadding =
                NEW_COLLECTION_DRAWABLE_PADDING.dpToPx(displayMetrics)
            holder.textView.setCompoundDrawablesWithIntrinsicBounds(
                ContextCompat.getDrawable(
                    holder.textView.context,
                    R.drawable.ic_new
                ), null, null, null
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

    companion object {
        private const val NEW_COLLECTION_PADDING_START = 24
        private const val NEW_COLLECTION_DRAWABLE_PADDING = 28
    }
}
