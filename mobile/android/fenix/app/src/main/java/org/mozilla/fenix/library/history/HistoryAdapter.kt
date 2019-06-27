/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import android.text.format.DateUtils
import android.view.LayoutInflater
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.history.viewholders.HistoryDeleteButtonViewHolder
import org.mozilla.fenix.library.history.viewholders.HistoryHeaderViewHolder
import org.mozilla.fenix.library.history.viewholders.HistoryListItemViewHolder
import java.util.Calendar
import java.util.Date

private sealed class AdapterItem {
    object DeleteButton : AdapterItem()
    data class SectionHeader(val range: Range) : AdapterItem()
    data class Item(val item: HistoryItem) : AdapterItem()
}

private enum class Range {
    Today, ThisWeek, ThisMonth, Older;

    fun humanReadable(context: Context): String = when (this) {
        Today -> context.getString(R.string.history_today)
        ThisWeek -> context.getString(R.string.history_this_week)
        ThisMonth -> context.getString(R.string.history_this_month)
        Older -> context.getString(R.string.history_older)
    }
}

private class HistoryList(val history: List<HistoryItem>) {
    val items: List<AdapterItem>

    init {
        val oneDayAgo = getDaysAgo(zero_days).time
        val sevenDaysAgo = getDaysAgo(seven_days).time
        val thirtyDaysAgo = getDaysAgo(thirty_days).time

        val lastWeek = LongRange(sevenDaysAgo, oneDayAgo)
        val lastMonth = LongRange(thirtyDaysAgo, sevenDaysAgo)
        val items = mutableListOf<AdapterItem>()
        items.add(AdapterItem.DeleteButton)

        val groups = history.groupBy { item ->
            when {
                DateUtils.isToday(item.visitedAt) -> Range.Today
                lastWeek.contains(item.visitedAt) -> Range.ThisWeek
                lastMonth.contains(item.visitedAt) -> Range.ThisMonth
                else -> Range.Older
            }
        }

        items.addAll(groups.adapterItemsForRange(Range.Today))
        items.addAll(groups.adapterItemsForRange(Range.ThisWeek))
        items.addAll(groups.adapterItemsForRange(Range.ThisMonth))
        items.addAll(groups.adapterItemsForRange(Range.Older))
        // No history only the delete button, so let's clear the list to show the empty text
        if (items.size == 1) items.clear()
        this.items = items
    }

    private fun Map<Range, List<HistoryItem>>.adapterItemsForRange(range: Range): List<AdapterItem> {
        return this[range]?.let { historyItems ->
            val items = mutableListOf<AdapterItem>()
            if (historyItems.isNotEmpty()) {
                items.add(AdapterItem.SectionHeader(range))
                for (item in historyItems) {
                    items.add(AdapterItem.Item(item))
                }
            }
            items
        } ?: listOf()
    }

    companion object {
        private const val zero_days = 0
        private const val seven_days = 7
        private const val thirty_days = 30

        private fun getDaysAgo(daysAgo: Int): Date {
            val calendar = Calendar.getInstance()
            calendar.add(Calendar.DAY_OF_YEAR, -daysAgo)

            return calendar.time
        }
    }
}

class HistoryAdapter(private val historyInteractor: HistoryInteractor) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    private var historyList: HistoryList = HistoryList(emptyList())
    private var mode: HistoryState.Mode = HistoryState.Mode.Normal
    var selected = listOf<HistoryItem>()

    fun updateData(items: List<HistoryItem>, mode: HistoryState.Mode) {
        val diffUtil = DiffUtil.calculateDiff(
            HistoryDiffUtil(
                this.historyList,
                HistoryList(items),
                HistoryList(selected),
                HistoryList((mode as? HistoryState.Mode.Editing)?.selectedItems ?: listOf()),
                this.mode,
                mode
            )
        )

        this.historyList = HistoryList(items)
        this.mode = mode
        this.selected = if (mode is HistoryState.Mode.Editing) mode.selectedItems else listOf()

        diffUtil.dispatchUpdatesTo(this)
    }

    private class HistoryDiffUtil(
        val old: HistoryList,
        val new: HistoryList,
        val oldSelected: HistoryList,
        val newSelected: HistoryList,
        val oldMode: HistoryState.Mode,
        val newMode: HistoryState.Mode
    ) : DiffUtil.Callback() {
        override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            old.items[oldItemPosition] == new.items[newItemPosition]

        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
            val modesEqual = oldMode::class == newMode::class
            val isStillSelected =
                oldSelected.items.contains(old.items[oldItemPosition]) &&
                        newSelected.items.contains(new.items[newItemPosition])
            val isStillNotSelected =
                !oldSelected.items.contains(old.items[oldItemPosition]) &&
                        !newSelected.items.contains(new.items[newItemPosition])
            return modesEqual && (isStillSelected || isStillNotSelected)
        }

        override fun getOldListSize(): Int = old.items.size
        override fun getNewListSize(): Int = new.items.size
    }

    override fun getItemCount(): Int = historyList.items.size

    override fun getItemViewType(position: Int): Int {
        return when (historyList.items[position]) {
            is AdapterItem.DeleteButton -> HistoryDeleteButtonViewHolder.LAYOUT_ID
            is AdapterItem.SectionHeader -> HistoryHeaderViewHolder.LAYOUT_ID
            is AdapterItem.Item -> HistoryListItemViewHolder.ID
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        return if (viewType == HistoryListItemViewHolder.ID) {
            val view = LibrarySiteItemView(parent.context).apply {
                layoutParams = ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT)
            }
            HistoryListItemViewHolder(view, historyInteractor)
        } else {
            val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
            when (viewType) {
                HistoryDeleteButtonViewHolder.LAYOUT_ID -> HistoryDeleteButtonViewHolder(view, historyInteractor)
                HistoryHeaderViewHolder.LAYOUT_ID -> HistoryHeaderViewHolder(view)
                else -> throw IllegalStateException()
            }
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is HistoryDeleteButtonViewHolder -> holder.bind(mode)
            is HistoryHeaderViewHolder -> historyList.items[position].also {
                if (it is AdapterItem.SectionHeader) {
                    holder.bind(it.range.humanReadable(holder.itemView.context))
                }
            }
            is HistoryListItemViewHolder -> (historyList.items[position] as AdapterItem.Item).also {
                holder.bind(it.item, mode)
            }
        }
    }
}
