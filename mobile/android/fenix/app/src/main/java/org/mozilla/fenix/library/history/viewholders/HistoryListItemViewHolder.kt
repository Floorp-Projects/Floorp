/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import android.widget.CompoundButton
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

    private val checkbox = view.should_remove_checkbox
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

        view.setOnClickListener {
            if (mode is HistoryState.Mode.Editing) {
                checkbox.isChecked = !checkbox.isChecked
                return@setOnClickListener
            }

            item?.apply {
                actionEmitter.onNext(HistoryAction.Select(this))
            }
        }

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

        checkbox.setOnCheckedChangeListener(checkListener)
    }

    fun bind(item: HistoryItem, mode: HistoryState.Mode) {
        this.item = item
        this.mode = mode

        title.text = item.title
        url.text = item.url

        val isEditing = mode is HistoryState.Mode.Editing
        checkbox.visibility = if (isEditing) View.VISIBLE else View.GONE
        favicon.visibility = if (isEditing) View.INVISIBLE else View.VISIBLE

        if (mode is HistoryState.Mode.Editing) {
            checkbox.setOnCheckedChangeListener(null)

            // Don't set the checkbox if it already contains the right value.
            // This prevent us from cutting off the animation
            val shouldCheck = mode.selectedItems.contains(item)
            if (checkbox.isChecked != shouldCheck) {
                checkbox.isChecked = mode.selectedItems.contains(item)
            }
            checkbox.setOnCheckedChangeListener(checkListener)
        }

        updateFavIcon(item.url)
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
            val bitmap = favicon.context.components.utils.icons
                .loadIcon(IconRequest(url)).await().bitmap
            launch(Dispatchers.Main) {
                favicon.setImageBitmap(bitmap)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_item
    }
}
