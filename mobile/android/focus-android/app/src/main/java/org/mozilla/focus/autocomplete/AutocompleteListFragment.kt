/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.app.Fragment
import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.view.*
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.TextView
import kotlinx.android.synthetic.main.fragment_autocomplete_customdomains.*
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.async
import kotlinx.coroutines.experimental.launch
import org.mozilla.focus.R
import org.mozilla.focus.settings.SettingsFragment
import java.util.*

/**
 * Fragment showing settings UI listing all custom autocomplete domains entered by the user.
 */
open class AutocompleteListFragment : Fragment() {
    /**
     * ItemTouchHelper for reordering items in the domain list.
     */
    val itemTouchHelper: ItemTouchHelper = ItemTouchHelper(object : SimpleCallback(ItemTouchHelper.UP or ItemTouchHelper.DOWN, 0) {
        override fun onMove(recyclerView: RecyclerView?, viewHolder: RecyclerView.ViewHolder?, target: RecyclerView.ViewHolder?): Boolean {
            if (recyclerView == null || viewHolder == null || target == null) {
                return false
            }

            val from = viewHolder.adapterPosition
            val to = target.adapterPosition

            (recyclerView.adapter as DomainListAdapter).move(from, to)

            return true
        }

        override fun onSwiped(viewHolder: RecyclerView.ViewHolder?, direction: Int) {}

        override fun getMovementFlags(recyclerView: RecyclerView?, viewHolder: RecyclerView.ViewHolder?): Int {
            if (viewHolder is AddActionViewHolder) {
                return ItemTouchHelper.Callback.makeMovementFlags(0,0)
            }

            return super.getMovementFlags(recyclerView, viewHolder)
        }

        override fun onSelectedChanged(viewHolder: RecyclerView.ViewHolder?, actionState: Int) {
            super.onSelectedChanged(viewHolder, actionState)

            if (viewHolder is DomainViewHolder) {
                viewHolder.onSelected()
            }
        }

        override fun clearView(recyclerView: RecyclerView?, viewHolder: RecyclerView.ViewHolder?) {
            super.clearView(recyclerView, viewHolder)

            if (viewHolder is DomainViewHolder) {
                viewHolder.onCleared()
            }
        }

        override fun canDropOver(recyclerView: RecyclerView?, current: RecyclerView.ViewHolder?, target: RecyclerView.ViewHolder?): Boolean {
            if (target is AddActionViewHolder) {
                return false
            }

            return super.canDropOver(recyclerView, current, target)
        }
    })

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    /**
     * In selection mode the user can select and remove items. In non-selection mode the list can
     * be reordered by the user.
     */
    open fun isSelectionMode() = false

    override fun onCreateView(inflater: LayoutInflater?, container: ViewGroup?, savedInstanceState: Bundle?): View =
            inflater!!.inflate(R.layout.fragment_autocomplete_customdomains, container, false)

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        domainList.layoutManager = LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false)
        domainList.adapter = DomainListAdapter()
        domainList.setHasFixedSize(true)

        if (!isSelectionMode()) {
            itemTouchHelper.attachToRecyclerView(domainList)
        }
    }

    override fun onResume() {
        super.onResume()

        val updater = activity as SettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_subitem_customlist)
        updater.updateIcon(R.drawable.ic_back)

        (domainList.adapter as DomainListAdapter).refresh(activity) {
            activity?.invalidateOptionsMenu()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu?, inflater: MenuInflater?) {
        inflater?.inflate(R.menu.menu_autocomplete_list, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu?) {
        menu?.findItem(R.id.remove)?.isVisible = isSelectionMode() || domainList.adapter.itemCount > 1
    }

    override fun onOptionsItemSelected(item: MenuItem?): Boolean = when (item?.itemId) {
        R.id.remove -> {
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.container, AutocompleteRemoveFragment())
                    .addToBackStack(null)
                    .commit()
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    /**
     * Adapter implementation for the list of custom autocomplete domains.
     */
    inner class DomainListAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
        private val domains: MutableList<String> = mutableListOf()
        private val selectedDomains: MutableList<String> = mutableListOf()

        fun refresh(context: Context, body: (() -> Unit)? = null) {
            launch(UI) {
                val updatedDomains = async { CustomAutocomplete.loadCustomAutoCompleteDomains(context) }.await()

                domains.clear()
                domains.addAll(updatedDomains)

                notifyDataSetChanged()

                body?.invoke()
            }
        }

        override fun getItemViewType(position: Int) =
                when (position) {
                    domains.size -> AddActionViewHolder.LAYOUT_ID
                    else -> DomainViewHolder.LAYOUT_ID
                }

        override fun onCreateViewHolder(parent: ViewGroup?, viewType: Int): RecyclerView.ViewHolder =
                when (viewType) {
                    AddActionViewHolder.LAYOUT_ID ->
                            AddActionViewHolder(
                                    this@AutocompleteListFragment,
                                    LayoutInflater.from(parent!!.context).inflate(viewType, parent, false))
                    DomainViewHolder.LAYOUT_ID ->
                            DomainViewHolder(
                                    LayoutInflater.from(parent!!.context).inflate(viewType, parent, false))
                    else -> throw IllegalArgumentException("Unknown view type: $viewType")
                }

        override fun getItemCount(): Int = domains.size + if (isSelectionMode()) 0 else 1

        override fun onBindViewHolder(holder: RecyclerView.ViewHolder?, position: Int) {
            if (holder is DomainViewHolder) {
                holder.bind(domains[position], isSelectionMode(), selectedDomains, itemTouchHelper)
            }
        }

        override fun onViewRecycled(holder: RecyclerView.ViewHolder?) {
            if (holder is DomainViewHolder) {
                holder.checkBoxView.setOnCheckedChangeListener(null)
            }
        }

        fun selection() : List<String> = selectedDomains

        fun move(from: Int, to: Int) {
            Collections.swap(domains, from, to)
            notifyItemMoved(from, to)

            launch(CommonPool) {
                CustomAutocomplete.saveDomains(activity.applicationContext, domains)
            }
        }
    }

    /**
     * ViewHolder implementation for a domain item in the list.
     */
    private class DomainViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val domainView: TextView = itemView.findViewById(R.id.domainView)
        val checkBoxView : CheckBox = itemView.findViewById(R.id.checkbox)
        val handleView : View = itemView.findViewById(R.id.handleView)

        companion object {
            val LAYOUT_ID = R.layout.item_custom_domain
        }

        fun bind(domain: String, isSelectionMode: Boolean, selectedDomains: MutableList<String>, itemTouchHelper: ItemTouchHelper) {
            domainView.text  = domain

            checkBoxView.visibility = if (isSelectionMode) View.VISIBLE else View.GONE
            checkBoxView.isChecked = selectedDomains.contains(domain)
            checkBoxView.setOnCheckedChangeListener({ _: CompoundButton, isChecked: Boolean ->
                if (isChecked) {
                    selectedDomains.add(domain)
                } else {
                    selectedDomains.remove(domain)
                }
            })

            handleView.visibility = if (isSelectionMode) View.GONE else View.VISIBLE
            handleView.setOnTouchListener(View.OnTouchListener { _, event ->
                if (event.actionMasked == MotionEvent.ACTION_DOWN) {
                    itemTouchHelper.startDrag(this)
                }
                false
            })

            if (isSelectionMode) {
                itemView.setOnClickListener({
                    checkBoxView.isChecked = !checkBoxView.isChecked
                })
            }
        }

        fun onSelected() {
            itemView.setBackgroundColor(Color.DKGRAY)
        }

        fun onCleared() {
            itemView.setBackgroundColor(0);
        }
    }

    /**
     * ViewHolder implementation for a "Add custom domain" item at the bottom of the list.
     */
    private class AddActionViewHolder(val fragment: AutocompleteListFragment, itemView: View) : RecyclerView.ViewHolder(itemView) {
        init {
            itemView.setOnClickListener {
                fragment.fragmentManager
                        .beginTransaction()
                        .replace(R.id.container, AutocompleteAddFragment())
                        .addToBackStack(null)
                        .commit()
            }
        }

        companion object {
            val LAYOUT_ID = R.layout.item_add_custom_domain
        }
    }
}