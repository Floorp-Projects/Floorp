/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.components.description
import org.mozilla.fenix.databinding.CollectionsListItemBinding
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.utils.view.ViewHolder

class SaveCollectionListAdapter(
    private val interactor: CollectionCreationInteractor
) : RecyclerView.Adapter<CollectionViewHolder>() {

    private var tabCollections = listOf<TabCollection>()
    private var selectedTabs: Set<Tab> = setOf()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CollectionViewHolder {
        val binding = CollectionsListItemBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )

        return CollectionViewHolder(binding)
    }

    override fun onBindViewHolder(holder: CollectionViewHolder, position: Int) {
        val collection = tabCollections[position]
        holder.bind(collection)
        holder.itemView.setOnClickListener {
            interactor.selectCollection(collection, selectedTabs.toList())
        }
    }

    override fun getItemCount(): Int = tabCollections.size

    fun updateData(tabCollections: List<TabCollection>, selectedTabs: Set<Tab>) {
        this.tabCollections = tabCollections
        this.selectedTabs = selectedTabs
        notifyDataSetChanged()
    }
}

class CollectionViewHolder(private val binding: CollectionsListItemBinding) : ViewHolder(binding.root) {

    fun bind(collection: TabCollection) {
        binding.collectionItem.text = collection.title
        binding.collectionDescription.text = collection.description(itemView.context)
        binding.collectionIcon.colorFilter =
            createBlendModeColorFilterCompat(collection.getIconColor(itemView.context), SRC_IN)
    }

    companion object {
        const val LAYOUT_ID = R.layout.collections_list_item
    }
}
