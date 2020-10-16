/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageButton
import androidx.core.content.ContextCompat
import kotlinx.android.synthetic.main.tab_tray_grid_item.view.*
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.browser.tabstray.thumbnail.TabThumbnailView
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.support.base.observer.Observable
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getMediaStateForSession
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.removeTouchDelegate
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.utils.Do
import kotlin.math.max

/**
 * A RecyclerView ViewHolder implementation for "tab" items.
 */
class TabTrayViewHolder(
    itemView: View,
    private val imageLoader: ImageLoader,
    private val store: BrowserStore = itemView.context.components.core.store,
    private val metrics: MetricController = itemView.context.components.analytics.metrics
) : TabViewHolder(itemView) {

    private val faviconView: ImageView? =
        itemView.findViewById(R.id.mozac_browser_tabstray_favicon_icon)
    private val titleView: TextView = itemView.findViewById(R.id.mozac_browser_tabstray_title)
    private val closeView: AppCompatImageButton =
        itemView.findViewById(R.id.mozac_browser_tabstray_close)
    private val thumbnailView: TabThumbnailView =
        itemView.findViewById(R.id.mozac_browser_tabstray_thumbnail)

    @VisibleForTesting
    internal val urlView: TextView? = itemView.findViewById(R.id.mozac_browser_tabstray_url)
    private val playPauseButtonView: ImageButton = itemView.findViewById(R.id.play_pause_button)

    override var tab: Tab? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    override fun bind(
        tab: Tab,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        observable: Observable<TabsTray.Observer>
    ) {
        this.tab = tab

        updateTitle(tab)
        updateUrl(tab)
        updateFavicon(tab)
        updateCloseButtonDescription(tab.title)
        updateSelectedTabIndicator(isSelected)

        if (tab.thumbnail != null) {
            thumbnailView.setImageBitmap(tab.thumbnail)
        } else {
            loadIntoThumbnailView(thumbnailView, tab.id)
        }

        // Media state
        playPauseButtonView.increaseTapArea(PLAY_PAUSE_BUTTON_EXTRA_DPS)
        with(playPauseButtonView) {
            invalidate()
            Do exhaustive when (store.state.getMediaStateForSession(tab.id)) {
                MediaState.State.PAUSED -> {
                    showAndEnable()
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_play)
                    setImageDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.media_state_play)
                    )
                }

                MediaState.State.PLAYING -> {
                    showAndEnable()
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_pause)
                    setImageDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.media_state_pause)
                    )
                }

                MediaState.State.NONE -> {
                    removeTouchDelegate()
                    removeAndDisable()
                }
            }
        }

        playPauseButtonView.setOnClickListener {
            Do exhaustive when (store.state.getMediaStateForSession(tab.id)) {
                MediaState.State.PLAYING -> {
                    metrics.track(Event.TabMediaPause)
                    store.state.media.pauseIfPlaying()
                }

                MediaState.State.PAUSED -> {
                    metrics.track(Event.TabMediaPlay)
                    store.state.media.playIfPaused()
                }

                MediaState.State.NONE -> throw AssertionError(
                    "Play/Pause button clicked without play/pause state."
                )
            }
        }

        closeView.setOnClickListener {
            observable.notifyObservers { onTabClosed(tab) }
        }
    }

    private fun updateFavicon(tab: Tab) {
        if (tab.icon != null) {
            faviconView?.visibility = View.VISIBLE
            faviconView?.setImageBitmap(tab.icon)
        } else {
            faviconView?.visibility = View.GONE
        }
    }

    private fun updateTitle(tab: Tab) {
        val title = if (tab.title.isNotEmpty()) {
            tab.title
        } else {
            tab.url
        }
        titleView.text = title
    }

    private fun updateUrl(tab: Tab) {
        // Truncate to MAX_URI_LENGTH to prevent the UI from locking up for
        // extremely large URLs such as data URIs or bookmarklets. The same
        // is done in the toolbar and awesomebar:
        // https://github.com/mozilla-mobile/fenix/issues/1824
        // https://github.com/mozilla-mobile/android-components/issues/6985
        urlView?.text = tab.url
            .toShortUrl(itemView.context.components.publicSuffixList)
            .take(MAX_URI_LENGTH)
    }

    @VisibleForTesting
    internal fun updateSelectedTabIndicator(isSelected: Boolean) {
        if (itemView.context.settings().gridTabView) {
            itemView.tab_tray_grid_item.background = if (isSelected) {
                AppCompatResources.getDrawable(itemView.context, R.drawable.tab_tray_grid_item_selected_border)
            } else {
                null
            }
            return
        }

        val color = if (isSelected) {
            R.color.tab_tray_item_selected_background_normal_theme
        } else {
            R.color.tab_tray_item_background_normal_theme
        }
        itemView.setBackgroundColor(
            ContextCompat.getColor(
                itemView.context,
                color
            )
        )
    }

    private fun updateCloseButtonDescription(title: String) {
        closeView.contentDescription =
            closeView.context.getString(R.string.close_tab_title, title)
    }

    private fun loadIntoThumbnailView(thumbnailView: ImageView, id: String) {
        val thumbnailSize = if (itemView.context.settings().gridTabView) {
            max(
                itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_height),
                itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_width)
            )
        } else {
            max(
                itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_height),
                itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_width)
            )
        }
        imageLoader.loadIntoView(thumbnailView, ImageLoadRequest(id, thumbnailSize))
    }

    internal fun updateAccessibilityRowInfo(item: View, newIndex: Int, isSelected: Boolean) {
        item.accessibilityDelegate = object : View.AccessibilityDelegate() {
            override fun onInitializeAccessibilityNodeInfo(
                host: View?,
                info: AccessibilityNodeInfo?
            ) {
                super.onInitializeAccessibilityNodeInfo(host, info)
                info?.collectionItemInfo =
                    AccessibilityNodeInfo.CollectionItemInfo.obtain(
                        newIndex,
                        1,
                        1,
                        1,
                        false,
                        isSelected
                    )
            }
        }
    }

    companion object {
        private const val PLAY_PAUSE_BUTTON_EXTRA_DPS = 24
    }
}
