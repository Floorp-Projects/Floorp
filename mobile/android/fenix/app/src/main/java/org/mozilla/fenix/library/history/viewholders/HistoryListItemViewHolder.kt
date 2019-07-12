/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import android.widget.CompoundButton
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.history_list_item.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.menu.BrowserMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.library.history.HistoryAction
import org.mozilla.fenix.library.history.HistoryItem
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.library.history.HistoryState
import kotlin.coroutines.CoroutineContext

class HistoryListItemViewHolder(
    view: View,
    private val actionEmitter: Observer<HistoryAction>,
    val job: Job
) : RecyclerView.ViewHolder(view), CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    private val favicon = view.history_favicon
    private val title = view.history_title
    private val url = view.history_url
    private val menuButton = view.history_item_overflow

    private var item: HistoryItem? = null
    private lateinit var historyMenu: HistoryItemMenu
    private var mode: HistoryState.Mode = HistoryState.Mode.Normal
    private val checkListener = CompoundButton.OnCheckedChangeListener { _, isChecked ->
        if (mode is HistoryState.Mode.Normal) {
            return@OnCheckedChangeListener
        }

        item?.apply {
            val action = if (isChecked) {
                HistoryAction.AddItemForRemoval(this)
            } else {
                HistoryAction.RemoveItemForRemoval(this)
            }

            actionEmitter.onNext(action)
        }
    }

    init {
        setupMenu()

        view.setOnLongClickListener {
            item?.apply {
                actionEmitter.onNext(HistoryAction.EnterEditMode(this))
            }

            true
        }

        menuButton.setOnClickListener {
            historyMenu.menuBuilder.build(view.context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN)
        }
    }

    fun bind(item: HistoryItem, mode: HistoryState.Mode) {
        this.item = item
        this.mode = mode

        title.text = item.title
        url.text = item.url

        val selected = when (mode) {
            is HistoryState.Mode.Editing -> mode.selectedItems.contains(item)
            else -> false
        }

        setClickListeners(item, selected)

        if (mode is HistoryState.Mode.Editing) {
            val backgroundTint =
                if (selected) {
                    ThemeManager.resolveAttribute(R.attr.accentHighContrast, itemView.context)
                } else {
                    ThemeManager.resolveAttribute(R.attr.neutral, itemView.context)
                }
            val backgroundTintList = ContextCompat.getColorStateList(itemView.context, backgroundTint)
            favicon.backgroundTintList = backgroundTintList

            if (selected) {
                favicon.setImageResource(R.drawable.mozac_ic_check)
            } else {
                updateFavIcon(item.url)
            }
        } else {
            val backgroundTint = ThemeManager.resolveAttribute(R.attr.neutral, itemView.context)
            val backgroundTintList = ContextCompat.getColorStateList(itemView.context, backgroundTint)
            favicon.backgroundTintList = backgroundTintList
            updateFavIcon(item.url)
        }
    }

    private fun setupMenu() {
        this.historyMenu = HistoryItemMenu(itemView.context) {
            when (it) {
                is HistoryItemMenu.Item.Delete -> {
                    item?.apply { actionEmitter.onNext(HistoryAction.Delete.One(this)) }
                }
            }
        }
    }

    private fun updateFavIcon(url: String) {
        launch(Dispatchers.IO) {
            val bitmap = favicon.context.components.core.icons
                .loadIcon(IconRequest(url)).await().bitmap
            launch(Dispatchers.Main) {
                favicon.setImageBitmap(bitmap)
            }
        }
    }

    private fun setClickListeners(
        item: HistoryItem,
        selected: Boolean
    ) {
        itemView.history_layout.setOnClickListener {
            if (mode == HistoryState.Mode.Normal) {
                actionEmitter.onNext(HistoryAction.Open(item))
            } else {
                if (selected) actionEmitter.onNext(HistoryAction.RemoveItemForRemoval(item)) else actionEmitter.onNext(
                    HistoryAction.AddItemForRemoval(item)
                )
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_item
    }
}
