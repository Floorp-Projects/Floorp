/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.feature.media.state.MediaState
import mozilla.components.support.ktx.android.util.dpToFloat
import org.jetbrains.anko.imageBitmap
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class TabViewHolder(
    view: View,
    actionEmitter: Observer<SessionControlAction>,
    override val containerView: View? = view
) :
    RecyclerView.ViewHolder(view), LayoutContainer {

    internal var tab: Tab? = null
    private var tabMenu: TabItemMenu

    init {
        tabMenu = TabItemMenu(view.context) {
            when (it) {
                is TabItemMenu.Item.Share ->
                    actionEmitter.onNext(TabAction.Share(tab?.sessionId!!))
            }
        }

        item_tab.setOnClickListener {
            actionEmitter.onNext(TabAction.Select(it, tab?.sessionId!!))
        }

        item_tab.setOnLongClickListener {
            view.context.components.analytics.metrics.track(Event.CollectionTabLongPressed)
            actionEmitter.onNext(TabAction.SaveTabGroup(tab?.sessionId!!))
            true
        }

        close_tab_button.setOnClickListener {
            actionEmitter.onNext(TabAction.Close(tab?.sessionId!!))
        }

        play_pause_button.increaseTapArea(PLAY_PAUSE_BUTTON_EXTRA_DPS)

        play_pause_button.setOnClickListener {
            when (tab?.mediaState) {
                is MediaState.Playing -> {
                    it.context.components.analytics.metrics.track(Event.TabMediaPlay)
                    actionEmitter.onNext(TabAction.PauseMedia(tab?.sessionId!!))
                }

                is MediaState.Paused -> {
                    it.context.components.analytics.metrics.track(Event.TabMediaPause)
                    actionEmitter.onNext(TabAction.PlayMedia(tab?.sessionId!!))
                }
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

    internal fun bindSession(tab: Tab) {
        updateTab(tab)
        updateTitle(tab.title)
        updateHostname(tab.hostname)
        updateFavIcon(tab.url, tab.icon)
        updateSelected(tab.selected ?: false)
        updatePlayPauseButton(tab.mediaState ?: MediaState.None)
        item_tab.transitionName = "$TAB_ITEM_TRANSITION_NAME${tab.sessionId}"
        updateCloseButtonDescription(tab.title)
    }

    internal fun updatePlayPauseButton(mediaState: MediaState) {
        with(play_pause_button) {
            visibility = if (mediaState is MediaState.Playing || mediaState is MediaState.Paused) {
                View.VISIBLE
            } else {
                View.GONE
            }

            if (mediaState is MediaState.Playing) {
                play_pause_button.contentDescription =
                    context.getString(R.string.mozac_feature_media_notification_action_pause)
                setImageDrawable(context.getDrawable(R.drawable.pause_with_background))
            } else {
                play_pause_button.contentDescription =
                    context.getString(R.string.mozac_feature_media_notification_action_play)
                setImageDrawable(context.getDrawable(R.drawable.play_with_background))
            }
        }
    }

    internal fun updateTab(tab: Tab) {
        this.tab = tab
    }
    internal fun updateTitle(text: String) {
        tab_title.text = text
    }

    internal fun updateHostname(text: String) {
        hostname.text = text
    }

    internal fun updateFavIcon(url: String, icon: Bitmap?) {
        if (icon == null) {
            favicon_image.context.components.core.icons.loadIntoView(favicon_image, url)
        } else {
            favicon_image.imageBitmap = icon
        }
    }

    internal fun updateSelected(selected: Boolean) {
        selected_border.visibility = if (selected) View.VISIBLE else View.GONE
    }
    internal fun updateCloseButtonDescription(title: String) {
        close_tab_button.contentDescription =
            close_tab_button.context.getString(R.string.close_tab_title, title)
    }

    companion object {
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        private const val PLAY_PAUSE_BUTTON_EXTRA_DPS = 24
        const val LAYOUT_ID = R.layout.tab_list_row
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
