/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.graphics.PorterDuff.Mode.SRC_IN
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.collections_list_item.view.*
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.components.description
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.home.Tab

class SaveCollectionListAdapter(
    private val interactor: CollectionCreationInteractor
) : RecyclerView.Adapter<CollectionViewHolder>() {

    private var tabCollections = listOf<TabCollection>()
    private var selectedTabs: Set<Tab> = setOf()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CollectionViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(CollectionViewHolder.LAYOUT_ID, parent, false)

        return CollectionViewHolder(view)
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

class CollectionViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    fun bind(collection: TabCollection) {
        itemView.collection_item.text = collection.title
        itemView.collection_description.text = collection.description(itemView.context)

        itemView.collection_icon.setColorFilter(
            collection.getIconColor(itemView.context),
            SRC_IN
        )
    }

    companion object {
        const val LAYOUT_ID = R.layout.collections_list_item
    }
}
