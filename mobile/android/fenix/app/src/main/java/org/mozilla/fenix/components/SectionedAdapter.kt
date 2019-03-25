package org.mozilla.fenix.components

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import java.lang.IllegalStateException

@SuppressWarnings("TooManyFunctions")
abstract class SectionedAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    sealed class SectionType {
        data class Header(val index: Int) : SectionType()
        data class Row(val section: Int, val row: Int) : SectionType()

        val viewType: Int
            get() = when (this) {
                is Header -> HeaderViewType
                is Row -> RowViewType
            }

        companion object {
            const val HeaderViewType = 0
            const val RowViewType = 1
        }
    }

    abstract fun numberOfSections(): Int
    abstract fun numberOfRowsInSection(section: Int): Int

    abstract fun onCreateHeaderViewHolder(parent: ViewGroup): RecyclerView.ViewHolder
    abstract fun onBindHeaderViewHolder(holder: RecyclerView.ViewHolder, header: SectionType.Header)
    abstract fun onCreateItemViewHolder(parent: ViewGroup): RecyclerView.ViewHolder
    abstract fun onBindItemViewHolder(holder: RecyclerView.ViewHolder, row: SectionType.Row)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        return when (viewType) {
            SectionType.HeaderViewType -> onCreateHeaderViewHolder(parent)
            SectionType.RowViewType -> onCreateItemViewHolder(parent)
            else -> throw IllegalStateException("ViewType: $viewType is invalid ")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return sectionTypeForPosition(position).viewType
    }

    final override fun getItemCount(): Int {
        var count = 0
        for (i in 0 until numberOfSections()) {
            count += numberOfRowsInSection(i) + 1
        }

        return count
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val sectionType = sectionTypeForPosition(position)

        when (sectionType) {
            is SectionType.Header -> onBindHeaderViewHolder(holder, sectionType)
            is SectionType.Row -> onBindItemViewHolder(holder, sectionType)
        }
    }

    private fun sectionTypeForPosition(position: Int): SectionType {
        var currentPosition = 0

        for (sectionIndex in 0 until numberOfSections()) {
            if (position == currentPosition) { return SectionType.Header(sectionIndex) }
            currentPosition += 1

            for (rowIndex in 0 until numberOfRowsInSection(sectionIndex)) {
                if (currentPosition == position) { return SectionType.Row(sectionIndex, rowIndex) }
                currentPosition += 1
            }
        }

        throw IllegalStateException("Position $position is out of bounds!")
    }
}
