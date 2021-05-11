/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageButton
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.checkbox_item.view.*
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.browser.tabstray.thumbnail.TabThumbnailView
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.removeTouchDelegate
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.selection.SelectionInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.isSelect

/**
 * A RecyclerView ViewHolder implementation for "tab" items.
 */
abstract class AbstractBrowserTabViewHolder(
    itemView: View,
    private val imageLoader: ImageLoader,
    private val trayStore: TabsTrayStore,
    private val selectionHolder: SelectionHolder<Tab>?,
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

    abstract val browserTrayInteractor: BrowserTrayInteractor
    abstract val thumbnailSize: Int

    override var tab: Tab? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    @Suppress("ComplexMethod", "LongMethod")
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
        updateMediaState(tab)

        if (selectionHolder != null) {
            setSelectionInteractor(tab, selectionHolder, browserTrayInteractor)
        } else {
            itemView.setOnClickListener { browserTrayInteractor.open(tab) }
        }

        if (tab.thumbnail != null) {
            thumbnailView.setImageBitmap(tab.thumbnail)
        } else {
            loadIntoThumbnailView(thumbnailView, tab.id)
        }
    }

    fun showTabIsMultiSelectEnabled(isSelected: Boolean) {
        itemView.selected_mask.isVisible = isSelected
        closeView.isInvisible = trayStore.state.mode is TabsTrayState.Mode.Select
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

    private fun updateCloseButtonDescription(title: String) {
        closeView.contentDescription =
            closeView.context.getString(R.string.close_tab_title, title)
    }

    /**
     * NB: Why do we query for the media state from the store, when we have [Tab.playbackState] and
     * [Tab.controller] already mapped?
     */
    private fun updateMediaState(tab: Tab) {
        // Media state
        playPauseButtonView.increaseTapArea(PLAY_PAUSE_BUTTON_EXTRA_DPS)

        with(playPauseButtonView) {
            invalidate()
            val sessionState = store.state.findTabOrCustomTab(tab.id)
            when (sessionState?.mediaSessionState?.playbackState) {
                MediaSession.PlaybackState.PAUSED -> {
                    showAndEnable()
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_play)
                    setImageDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.media_state_play)
                    )
                }

                MediaSession.PlaybackState.PLAYING -> {
                    showAndEnable()
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_pause)
                    setImageDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.media_state_pause)
                    )
                }

                else -> {
                    removeTouchDelegate()
                    removeAndDisable()
                }
            }

            setOnClickListener {
                when (sessionState?.mediaSessionState?.playbackState) {
                    MediaSession.PlaybackState.PLAYING -> {
                        metrics.track(Event.TabMediaPause)
                        sessionState.mediaSessionState?.controller?.pause()
                    }

                    MediaSession.PlaybackState.PAUSED -> {
                        metrics.track(Event.TabMediaPlay)
                        sessionState.mediaSessionState?.controller?.play()
                    }
                    else -> throw AssertionError(
                        "Play/Pause button clicked without play/pause state."
                    )
                }
            }
        }
    }

    private fun loadIntoThumbnailView(thumbnailView: ImageView, id: String) {
        imageLoader.loadIntoView(thumbnailView, ImageLoadRequest(id, thumbnailSize))
    }

    private fun setSelectionInteractor(
        item: Tab,
        holder: SelectionHolder<Tab>,
        interactor: SelectionInteractor<Tab>
    ) {
        itemView.setOnClickListener {
            val selected = holder.selectedItems
            when {
                selected.isEmpty() && trayStore.state.mode.isSelect().not() -> interactor.open(item)
                item in selected -> interactor.deselect(item)
                else -> interactor.select(item)
            }
        }

        itemView.setOnLongClickListener {
            if (holder.selectedItems.isEmpty()) {
                metrics.track(Event.CollectionTabLongPressed)
                interactor.select(item)
                true
            } else {
                false
            }
        }
    }

    companion object {
        internal const val PLAY_PAUSE_BUTTON_EXTRA_DPS = 24
        internal const val GRID_ITEM_CLOSE_BUTTON_EXTRA_DPS = 24
    }
}
