package org.mozilla.fenix.components

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import java.lang.IllegalStateException

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
        return when  (viewType) {
            SectionType.HeaderViewType -> onCreateHeaderViewHolder(parent)
            SectionType.RowViewType -> onCreateItemViewHolder(parent)
            else -> throw IllegalStateException("ViewType: $viewType is invalid ")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return sectionTypeForPosition(position).viewType
    }

    override fun getItemCount(): Int {
        val numberOfSections = numberOfSections()
        return numberOfSections + (0..numberOfSections).reduce { a, b -> a + numberOfRowsInSection(b) }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val sectionType = sectionTypeForPosition(position)

        when (sectionType) {
            is SectionType.Header -> onBindHeaderViewHolder(holder, sectionType)
            is SectionType.Row -> onBindItemViewHolder(holder, sectionType)
        }
    }

    private fun sectionTypeForPosition(position: Int): SectionType {
        var counter = 0

        for (section in 0..numberOfSections()) {
            if (counter == position) { return SectionType.Header(section) }
            counter += 1

            for (row in 0..numberOfRowsInSection(section)) {
                if (counter == position) { return SectionType.Row(section, row) }
                counter += 1
            }
        }
    }
}