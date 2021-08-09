/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isGone
import androidx.core.view.isInvisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.CollectionTabListRowBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.utils.view.ViewHolder

class CollectionCreationTabListAdapter(
    private val interactor: CollectionCreationInteractor
) : RecyclerView.Adapter<TabViewHolder>() {

    private var tabs: List<Tab> = listOf()
    private var selectedTabs: MutableSet<Tab> = mutableSetOf()
    private var hideCheckboxes = false

    private lateinit var binding: CollectionTabListRowBinding

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        binding = CollectionTabListRowBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )

        return TabViewHolder(binding)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int, payloads: MutableList<Any>) {
        if (payloads.isEmpty()) {
            super.onBindViewHolder(holder, position, payloads)
        } else {
            when (payloads[0]) {
                is CheckChanged -> {
                    val checkChanged = payloads[0] as CheckChanged
                    if (checkChanged.shouldBeChecked) {
                        binding.tabSelectedCheckbox.isChecked = true
                    } else if (checkChanged.shouldBeUnchecked) {
                        binding.tabSelectedCheckbox.isChecked = false
                    }
                    binding.tabSelectedCheckbox.isGone = checkChanged.shouldHideCheckBox
                }
            }
        }
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tab = tabs[position]
        val isSelected = selectedTabs.contains(tab)
        binding.tabSelectedCheckbox.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                selectedTabs.add(tab)
                interactor.addTabToSelection(tab)
            } else {
                selectedTabs.remove(tab)
                interactor.removeTabFromSelection(tab)
            }
        }
        holder.bind(tab, isSelected, hideCheckboxes)
    }

    override fun getItemCount(): Int = tabs.size

    fun updateData(tabs: List<Tab>, selectedTabs: Set<Tab>, hideCheckboxes: Boolean = false) {
        val diffUtil = DiffUtil.calculateDiff(
            TabDiffUtil(
                this.tabs,
                tabs,
                this.selectedTabs,
                selectedTabs,
                this.hideCheckboxes,
                hideCheckboxes
            )
        )

        this.tabs = tabs
        this.selectedTabs = selectedTabs.toMutableSet()
        this.hideCheckboxes = hideCheckboxes

        diffUtil.dispatchUpdatesTo(this)
    }
}

class TabViewHolder(private val binding: CollectionTabListRowBinding) : ViewHolder(binding.root) {

    init {
        binding.collectionItemTab.setOnClickListener {
            binding.tabSelectedCheckbox.isChecked = !binding.tabSelectedCheckbox.isChecked
        }
    }

    fun bind(tab: Tab, isSelected: Boolean, shouldHideCheckBox: Boolean) {
        binding.hostname.text = tab.hostname
        binding.tabTitle.text = tab.title
        binding.tabSelectedCheckbox.isInvisible = shouldHideCheckBox
        itemView.isClickable = !shouldHideCheckBox
        if (binding.tabSelectedCheckbox.isChecked != isSelected) {
            binding.tabSelectedCheckbox.isChecked = isSelected
        }

        itemView.context.components.core.icons.loadIntoView(binding.faviconImage, tab.url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.collection_tab_list_row
    }
}
