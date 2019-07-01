package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.collection_tab_list_row.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.utils.AdapterWithJob
import kotlin.coroutines.CoroutineContext

class CollectionCreationTabListAdapter(
    val actionEmitter: Observer<CollectionCreationAction>
) : AdapterWithJob<TabViewHolder>() {
    private var tabs: List<Tab> = listOf()
    private var selectedTabs: MutableSet<Tab> = mutableSetOf()
    private var hideCheckboxes = false

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        val view =
            LayoutInflater.from(parent.context).inflate(TabViewHolder.LAYOUT_ID, parent, false)

        return TabViewHolder(view, adapterJob)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int, payloads: MutableList<Any>) {
        if (payloads.isEmpty()) {
            super.onBindViewHolder(holder, position, payloads)
        } else {
            when (payloads[0]) {
                is CheckChanged -> {
                    val checkChanged = payloads[0] as CheckChanged
                    if (checkChanged.shouldBeChecked) {
                        holder.view.tab_selected_checkbox.isChecked = true
                    } else if (checkChanged.shouldBeUnchecked) {
                        holder.view.tab_selected_checkbox.isChecked = false
                    }
                    holder.view.tab_selected_checkbox.visibility =
                        if (checkChanged.shouldHideCheckBox) View.GONE else View.VISIBLE
                }
            }
        }
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tab = tabs[position]
        val isSelected = selectedTabs.contains(tab)
        holder.view.tab_selected_checkbox.setOnCheckedChangeListener { _, isChecked ->
            val action = if (isChecked) {
                selectedTabs.add(tab)
                CollectionCreationAction.AddTabToSelection(tab)
            } else {
                selectedTabs.remove(tab)
                CollectionCreationAction.RemoveTabFromSelection(tab)
            }
            actionEmitter.onNext(action)
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

private class TabDiffUtil(
    val old: List<Tab>,
    val new: List<Tab>,
    val oldSelected: Set<Tab>,
    val newSelected: Set<Tab>,
    val oldHideCheckboxes: Boolean,
    val newHideCheckboxes: Boolean
) : DiffUtil.Callback() {
    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        old[oldItemPosition].sessionId == new[newItemPosition].sessionId

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
        val isSameTab = old[oldItemPosition].sessionId == new[newItemPosition].sessionId
        val sameSelectedState = oldSelected.contains(old[oldItemPosition]) == newSelected.contains(new[newItemPosition])
        val isSameHideCheckboxes = oldHideCheckboxes == newHideCheckboxes
        return isSameTab && sameSelectedState && isSameHideCheckboxes
    }

    override fun getChangePayload(oldItemPosition: Int, newItemPosition: Int): Any? {
        val shouldBeChecked = newSelected.contains(new[newItemPosition]) && !oldSelected.contains(old[oldItemPosition])
        val shouldBeUnchecked =
            !newSelected.contains(new[newItemPosition]) && oldSelected.contains(old[oldItemPosition])
        return CheckChanged(shouldBeChecked, shouldBeUnchecked, newHideCheckboxes)
    }

    override fun getOldListSize(): Int = old.size
    override fun getNewListSize(): Int = new.size
}

data class CheckChanged(val shouldBeChecked: Boolean, val shouldBeUnchecked: Boolean, val shouldHideCheckBox: Boolean)

class TabViewHolder(
    val view: View,
    val job: Job
) :
    RecyclerView.ViewHolder(view), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    private var tab: Tab? = null
    private val checkbox = view.tab_selected_checkbox!!

    init {
        view.collection_item_tab.setOnClickListener {
            checkbox.isChecked = !checkbox.isChecked
        }
    }

    fun bind(tab: Tab, isSelected: Boolean, shouldHideCheckBox: Boolean) {
        this.tab = tab
        view.hostname.text = tab.hostname
        view.tab_title.text = tab.title
        checkbox.visibility = if (shouldHideCheckBox) View.INVISIBLE else View.VISIBLE
        view.isClickable = !shouldHideCheckBox
        if (checkbox.isChecked != isSelected) {
            checkbox.isChecked = isSelected
        }

        launch(Dispatchers.IO) {
            val bitmap = view.favicon_image.context.components.core.icons
                .loadIcon(IconRequest(tab.url)).await().bitmap
            launch(Dispatchers.Main) {
                view.favicon_image.setImageBitmap(bitmap)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.collection_tab_list_row
    }
}
