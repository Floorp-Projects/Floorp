/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy.studies

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter

class StudiesAdapter(var removeStudyListener: (StudiesListItem.ActiveStudy) -> Unit = {}) :
    ListAdapter<StudiesListItem, StudiesViewHolder>(StudiesDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): StudiesViewHolder {
        return when (viewType) {
            ACTIVE_STUDY_LAYOUT_ID -> {
                val rootView = LayoutInflater.from(parent.context)
                    .inflate(ACTIVE_STUDY_LAYOUT_ID, parent, false) as ConstraintLayout
                StudiesViewHolder.ActiveStudiesViewHolder(rootView)
            }
            SECTION_LAYOUT_ID -> {
                val rootView = LayoutInflater.from(parent.context)
                    .inflate(SECTION_LAYOUT_ID, parent, false) as LinearLayout
                StudiesViewHolder.SectionViewHolder(rootView)
            }
            else -> throw IllegalArgumentException("Unrecognized viewType")
        }
    }

    override fun onBindViewHolder(holder: StudiesViewHolder, position: Int) {
        val item = getItem(position)

        when (holder) {
            is StudiesViewHolder.SectionViewHolder -> holder.bindSection(item as StudiesListItem.Section)
            is StudiesViewHolder.ActiveStudiesViewHolder -> holder.bindStudy(
                item as StudiesListItem.ActiveStudy,
                removeStudyListener,
            )
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (getItem(position)) {
            is StudiesListItem.Section -> SECTION_LAYOUT_ID
            is StudiesListItem.ActiveStudy -> ACTIVE_STUDY_LAYOUT_ID
        }
    }

    class StudiesDiffCallback : DiffUtil.ItemCallback<StudiesListItem>() {
        override fun areItemsTheSame(oldItem: StudiesListItem, newItem: StudiesListItem): Boolean {
            return when {
                oldItem is StudiesListItem.ActiveStudy && newItem is StudiesListItem.ActiveStudy -> {
                    oldItem.value.slug == newItem.value.slug
                }
                oldItem is StudiesListItem.Section && newItem is StudiesListItem.Section -> {
                    oldItem.titleId == newItem.titleId
                }
                else -> false
            }
        }

        override fun areContentsTheSame(oldItem: StudiesListItem, newItem: StudiesListItem) =
            oldItem == newItem
    }
}
