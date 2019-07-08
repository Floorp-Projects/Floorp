/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.content.Context
import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.support.ktx.android.util.dpToFloat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext
import kotlin.coroutines.CoroutineContext

class TabViewHolder(
    view: View,
    actionEmitter: Observer<SessionControlAction>,
    private val job: Job,
    override val containerView: View? = view
) :
    RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    var tab: Tab? = null
    private var tabMenu: TabItemMenu

    init {
        tabMenu = TabItemMenu(view.context) {
            when (it) {
                is TabItemMenu.Item.Share ->
                    actionEmitter.onNext(TabAction.Share(tab?.sessionId!!))
            }
        }

        close_tab_button.increaseTapArea(buttonIncreaseDps)
        item_tab.setOnClickListener {
            actionEmitter.onNext(TabAction.Select(it, tab?.sessionId!!))
        }

        item_tab.setOnLongClickListener {
            actionEmitter.onNext(TabAction.SaveTabGroup(tab?.sessionId!!))
            true
        }

        close_tab_button?.run {
            increaseTapArea(buttonIncreaseDps)
            setOnClickListener {
                actionEmitter.onNext(TabAction.Close(tab?.sessionId!!))
            }
        }

        favicon_image.clipToOutline = true
        favicon_image.outlineProvider = object : ViewOutlineProvider() {
            override fun getOutline(view: View?, outline: Outline?) {
                outline?.setRoundRect(
                    0,
                    0,
                    view!!.width,
                    view.height,
                    favIconBorderRadiusInPx.dpToFloat(view.context.resources.displayMetrics)
                )
            }
        }
    }

    fun bindSession(tab: Tab) {
        this.tab = tab
        updateTabUI(tab)
        item_tab.transitionName = "$TAB_ITEM_TRANSITION_NAME${tab.sessionId}"
        updateSelected(tab.selected ?: false)
    }

    private fun updateTabUI(tab: Tab) {
        hostname.text = tab.hostname
        tab_title.text = tab.title
        launch(Dispatchers.IO) {
            val bitmap = favicon_image.context.components.core.icons
                .loadIcon(IconRequest(tab.url)).await().bitmap
            launch(Dispatchers.Main) {
                favicon_image.setImageBitmap(bitmap)
            }
        }
    }

    fun updateSelected(selected: Boolean) {
        selected_border.visibility = if (selected) View.VISIBLE else View.GONE
    }

    companion object {
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        const val LAYOUT_ID = R.layout.tab_list_row
        const val buttonIncreaseDps = 12
        const val favIconBorderRadiusInPx = 4
    }
}

class TabItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object Share : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.tab_share)
            ) {
                onItemTapped.invoke(Item.Share)
            }
        )
    }
}
