package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
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
import kotlin.coroutines.CoroutineContext

class CollectionCreationTabListAdapter(
    val actionEmitter: Observer<CollectionCreationAction>
) : RecyclerView.Adapter<TabViewHolder>() {

    private var tabs: List<Tab> = listOf()
    private var selectedTabs: Set<Tab> = setOf()
    private lateinit var job: Job
    private var hideCheckboxes = false

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        val view =
            LayoutInflater.from(parent.context).inflate(TabViewHolder.LAYOUT_ID, parent, false)

        return TabViewHolder(view, actionEmitter, job)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tab = tabs[position]
        val isSelected = selectedTabs.contains(tab)
        holder.bind(tab, isSelected, hideCheckboxes)
    }

    override fun getItemCount(): Int = tabs.size

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
    }

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
        this.selectedTabs = selectedTabs
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
        val isSameTab = old[oldItemPosition].url == new[newItemPosition].url
        val sameSelectedState =
            oldSelected.contains(old[oldItemPosition]) == newSelected.contains(new[newItemPosition])
        val isSameHideCheckboxes = oldHideCheckboxes == newHideCheckboxes
        return isSameTab && sameSelectedState && isSameHideCheckboxes
    }

    override fun getOldListSize(): Int = old.size
    override fun getNewListSize(): Int = new.size
}

class TabViewHolder(
    val view: View,
    actionEmitter: Observer<CollectionCreationAction>,
    val job: Job
) :
    RecyclerView.ViewHolder(view), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    private var tab: Tab? = null
    private val checkbox = view.tab_selected_checkbox!!
    private val checkboxListener = CompoundButton.OnCheckedChangeListener { _, isChecked ->
        tab?.apply {
            val action = if (isChecked) CollectionCreationAction.AddTabToSelection(this)
            else CollectionCreationAction.RemoveTabFromSelection(this)

            actionEmitter.onNext(action)
        }
    }

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
        checkbox.setOnCheckedChangeListener(null)
        if (checkbox.isChecked != isSelected) {
            checkbox.isChecked = isSelected
        }
        checkbox.setOnCheckedChangeListener(checkboxListener)

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
