/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate

internal class MenuCandidateListAdapter(
    private val inflater: LayoutInflater,
    private val dismiss: () -> Unit,
    private val reopenMenu: (NestedMenuCandidate) -> Unit,
) : ListAdapter<MenuCandidate, MenuCandidateViewHolder<out MenuCandidate>>(MenuCandidateDiffer) {

    @LayoutRes
    override fun getItemViewType(position: Int) = when (val item = getItem(position)) {
        is TextMenuCandidate -> TextMenuCandidateViewHolder.layoutResource
        is DecorativeTextMenuCandidate -> DecorativeTextMenuCandidateViewHolder.layoutResource
        is CompoundMenuCandidate -> CompoundMenuCandidateViewHolder.getLayoutResource(item)
        is NestedMenuCandidate -> NestedMenuCandidateViewHolder.layoutResource
        is RowMenuCandidate -> RowMenuCandidateViewHolder.layoutResource
        is DividerMenuCandidate -> DividerMenuCandidateViewHolder.layoutResource
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        @LayoutRes viewType: Int,
    ): MenuCandidateViewHolder<out MenuCandidate> {
        val view = inflater.inflate(viewType, parent, false)
        return when (viewType) {
            TextMenuCandidateViewHolder.layoutResource ->
                TextMenuCandidateViewHolder(view, inflater, dismiss)
            DecorativeTextMenuCandidateViewHolder.layoutResource ->
                DecorativeTextMenuCandidateViewHolder(view, inflater)
            CompoundCheckboxMenuCandidateViewHolder.layoutResource ->
                CompoundCheckboxMenuCandidateViewHolder(view, inflater, dismiss)
            CompoundSwitchMenuCandidateViewHolder.layoutResource ->
                CompoundSwitchMenuCandidateViewHolder(view, inflater, dismiss)
            NestedMenuCandidateViewHolder.layoutResource ->
                NestedMenuCandidateViewHolder(view, inflater, dismiss, reopenMenu)
            RowMenuCandidateViewHolder.layoutResource ->
                RowMenuCandidateViewHolder(view, inflater, dismiss)
            DividerMenuCandidateViewHolder.layoutResource ->
                DividerMenuCandidateViewHolder(view, inflater)
            else -> throw IllegalArgumentException("Invalid viewType $viewType")
        }
    }

    override fun onBindViewHolder(holder: MenuCandidateViewHolder<out MenuCandidate>, position: Int) {
        val item = getItem(position)
        when (holder) {
            is TextMenuCandidateViewHolder -> holder.bind(item as TextMenuCandidate)
            is DecorativeTextMenuCandidateViewHolder -> holder.bind(item as DecorativeTextMenuCandidate)
            is CompoundMenuCandidateViewHolder -> holder.bind(item as CompoundMenuCandidate)
            is NestedMenuCandidateViewHolder -> holder.bind(item as NestedMenuCandidate)
            is RowMenuCandidateViewHolder -> holder.bind(item as RowMenuCandidate)
            is DividerMenuCandidateViewHolder -> holder.bind(item as DividerMenuCandidate)
        }
    }
}

private object MenuCandidateDiffer : DiffUtil.ItemCallback<MenuCandidate>() {
    override fun areItemsTheSame(oldItem: MenuCandidate, newItem: MenuCandidate) =
        oldItem::class == newItem::class

    @Suppress("DiffUtilEquals")
    override fun areContentsTheSame(oldItem: MenuCandidate, newItem: MenuCandidate) =
        oldItem == newItem
}
