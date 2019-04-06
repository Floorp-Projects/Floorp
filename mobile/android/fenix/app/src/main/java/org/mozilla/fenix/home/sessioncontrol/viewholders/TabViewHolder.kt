/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import org.jetbrains.anko.image
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext
import kotlin.coroutines.CoroutineContext

class TabViewHolder(
    val view: View,
    actionEmitter: Observer<SessionControlAction>,
    val job: Job,
    override val containerView: View? = view
) :
    RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    var tab: Tab? = null

    init {
        item_tab.setOnClickListener {
            actionEmitter.onNext(TabAction.Select(tab?.sessionId!!))
        }

        close_tab_button?.run {
            increaseTapArea(closeButtonIncreaseDps)
            setOnClickListener {
                actionEmitter.onNext(TabAction.Close(tab?.sessionId!!))
            }
        }
    }

    fun bindSession(tab: Tab, position: Int) {
        this.tab = tab
        updateTabBackground(position)
        updateUrl(tab.url)
        updateSelected(tab.selected)
    }

    fun updateUrl(url: String) {
        text_url.text = url
        launch(Dispatchers.IO) {
            val bitmap = favicon_image.context.components.utils.icons
                .loadIcon(IconRequest(url)).await().bitmap
            launch(Dispatchers.Main) {
                favicon_image.setImageBitmap(bitmap)
            }
        }
    }

    fun updateSelected(selected: Boolean) {
        selected_border.visibility = if (selected) View.VISIBLE else View.GONE
    }

    fun updateTabBackground(id: Int) {
        if (tab?.thumbnail != null) {
            tab_background.setImageBitmap(tab?.thumbnail)
        } else {
            val background = availableBackgrounds[id % availableBackgrounds.size]
            tab_background.image = ContextCompat.getDrawable(view.context, background)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.tab_list_row
        const val closeButtonIncreaseDps = 12

        private val availableBackgrounds = listOf(
            R.drawable.sessions_01, R.drawable.sessions_02,
            R.drawable.sessions_03, R.drawable.sessions_06,
            R.drawable.sessions_07, R.drawable.sessions_08
        )
    }
}
