/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.graphics.Bitmap
import android.graphics.Outline
import android.view.View
import android.view.ViewOutlineProvider
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import mozilla.components.browser.state.state.MediaState
import mozilla.components.support.ktx.android.util.dpToFloat
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.removeTouchDelegate
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.home.sessioncontrol.TabSessionInteractor

class TabViewHolder(
    view: View,
    interactor: TabSessionInteractor,
    override val containerView: View? = view
) :
    RecyclerView.ViewHolder(view), LayoutContainer {

    internal var tab: Tab? = null

    init {
        item_tab.setOnClickListener {
            interactor.onSelectTab(it, tab?.sessionId!!)
        }

        item_tab.setOnLongClickListener {
            view.context.components.analytics.metrics.track(Event.CollectionTabLongPressed)
            interactor.onSaveToCollection(tab?.sessionId!!)
            return@setOnLongClickListener true
        }

        close_tab_button.setOnClickListener {
            interactor.onCloseTab(tab?.sessionId!!)
        }

        play_pause_button.setOnClickListener {
            when (tab?.mediaState) {
                MediaState.State.PLAYING -> {
                    it.context.components.analytics.metrics.track(Event.TabMediaPause)
                    interactor.onPauseMediaClicked()
                }

                MediaState.State.PAUSED -> {
                    it.context.components.analytics.metrics.track(Event.TabMediaPlay)
                    interactor.onPlayMediaClicked()
                }

                MediaState.State.NONE -> throw AssertionError(
                    "Play/Pause button clicked without play/pause state."
                )
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
        updatePlayPauseButton(tab.mediaState)
        item_tab.transitionName = "$TAB_ITEM_TRANSITION_NAME${tab.sessionId}"
        updateCloseButtonDescription(tab.title)
    }

    internal fun updatePlayPauseButton(mediaState: MediaState.State) {
        with(play_pause_button) {
            invalidate()
            when (mediaState) {
                MediaState.State.PAUSED -> {
                    showAndEnable()
                    play_pause_button.increaseTapArea(PLAY_PAUSE_BUTTON_EXTRA_DPS)
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_play)
                    setImageDrawable(
                        AppCompatResources.getDrawable(
                            context,
                            R.drawable.play_with_background
                        )
                    )
                }

                MediaState.State.PLAYING -> {
                    showAndEnable()
                    play_pause_button.increaseTapArea(PLAY_PAUSE_BUTTON_EXTRA_DPS)
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_pause)
                    setImageDrawable(
                        AppCompatResources.getDrawable(
                            context,
                            R.drawable.pause_with_background
                        )
                    )
                }

                MediaState.State.NONE -> {
                    removeTouchDelegate()
                    removeAndDisable()
                }
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
            favicon_image.setImageBitmap(icon)
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
