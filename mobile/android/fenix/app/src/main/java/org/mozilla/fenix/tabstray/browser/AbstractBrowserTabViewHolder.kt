/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.annotation.SuppressLint
import android.graphics.PointF
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageButton
import androidx.core.view.ViewCompat
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.SelectableTabViewHolder
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.browser.tabstray.thumbnail.TabThumbnailView
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.ktx.kotlin.MAX_URI_LENGTH
import mozilla.components.support.ktx.kotlin.toShortUrl
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.removeTouchDelegate
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.toDisplayTitle

/**
 * A RecyclerView ViewHolder implementation for "tab" items.
 *
 * @param itemView [View] that displays a "tab".
 * @param imageLoader [ImageLoader] used to load tab thumbnails.
 * @param trayStore [TabsTrayStore] containing the complete state of tabs tray and methods to update that.
 * @param selectionHolder [SelectionHolder] instance containing the selected tabs in the tabs tray.
 * @property featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param store [BrowserStore] containing the complete state of the browser and methods to update that.
 */
@Suppress("LongParameterList")
abstract class AbstractBrowserTabViewHolder(
    itemView: View,
    private val imageLoader: ImageLoader,
    private val trayStore: TabsTrayStore,
    private val selectionHolder: SelectionHolder<TabSessionState>?,
    internal val featureName: String,
    private val store: BrowserStore = itemView.context.components.core.store,
) : SelectableTabViewHolder(itemView) {

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

    abstract val interactor: TabsTrayInteractor
    abstract val thumbnailSize: Int

    override var tab: TabSessionState? = null

    internal var beingDragged: Boolean = false
    private var touchStartPoint: PointF? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    @Suppress("ComplexMethod", "LongMethod")
    override fun bind(
        tab: TabSessionState,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        delegate: TabsTray.Delegate,
    ) {
        this.tab = tab
        beingDragged = false

        updateTitle(tab)
        updateUrl(tab)
        updateFavicon(tab)
        updateCloseButtonDescription(tab.content.title)
        updateSelectedTabIndicator(isSelected)
        updateMediaState(tab)

        if (selectionHolder != null) {
            setSelectionInteractor(tab, selectionHolder, interactor)
        } else {
            itemView.setOnClickListener {
                interactor.onTabSelected(tab, featureName)
            }
        }

        loadIntoThumbnailView(thumbnailView, tab.id, tab.content.private)
    }

    override fun showTabIsMultiSelectEnabled(selectedMaskView: View?, isSelected: Boolean) {
        selectedMaskView?.isVisible = isSelected
        closeView.isInvisible = trayStore.state.mode is TabsTrayState.Mode.Select
        closeView.isClickable = trayStore.state.mode !is TabsTrayState.Mode.Select
    }

    private fun updateFavicon(tab: TabSessionState) {
        if (tab.content.icon != null) {
            faviconView?.visibility = View.VISIBLE
            faviconView?.setImageBitmap(tab.content.icon)
        } else {
            faviconView?.visibility = View.GONE
        }
    }

    private fun updateTitle(tab: TabSessionState) {
        // We can use the max URI length for titles as well.
        val title = tab.toDisplayTitle().take(MAX_URI_LENGTH)
        titleView.text = title
    }

    private fun updateUrl(tab: TabSessionState) {
        // Truncate to MAX_URI_LENGTH to prevent the UI from locking up for
        // extremely large URLs such as data URIs or bookmarklets. The same
        // is done in the toolbar and awesomebar:
        // https://github.com/mozilla-mobile/fenix/issues/1824
        // https://github.com/mozilla-mobile/android-components/issues/6985
        urlView?.text = tab.content.url
            .toShortUrl(itemView.context.components.publicSuffixList)
            .take(MAX_URI_LENGTH)
    }

    private fun updateCloseButtonDescription(title: String) {
        closeView.contentDescription =
            closeView.context.getString(R.string.close_tab_title, title)
    }

    private fun updateMediaState(tab: TabSessionState) {
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
                        AppCompatResources.getDrawable(context, R.drawable.media_state_play),
                    )
                }

                MediaSession.PlaybackState.PLAYING -> {
                    showAndEnable()
                    contentDescription =
                        context.getString(R.string.mozac_feature_media_notification_action_pause)
                    setImageDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.media_state_pause),
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
                        Tab.mediaPause.record(NoExtras())
                        sessionState.mediaSessionState?.controller?.pause()
                    }

                    MediaSession.PlaybackState.PAUSED -> {
                        Tab.mediaPlay.record(NoExtras())
                        sessionState.mediaSessionState?.controller?.play()
                    }
                    else -> throw AssertionError(
                        "Play/Pause button clicked without play/pause state.",
                    )
                }
            }
        }
    }

    private fun loadIntoThumbnailView(thumbnailView: ImageView, id: String, isPrivate: Boolean) {
        imageLoader.loadIntoView(thumbnailView, ImageLoadRequest(id, thumbnailSize, isPrivate))
    }

    private fun setSelectionInteractor(
        item: TabSessionState,
        holder: SelectionHolder<TabSessionState>,
        interactor: TabsTrayInteractor,
    ) {
        itemView.setOnClickListener {
            interactor.onTabSelected(item, featureName)
        }

        itemView.setOnLongClickListener {
            interactor.onTabLongClicked(item)
        }
        setDragInteractor(item, holder, interactor)
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setDragInteractor(
        item: TabSessionState,
        holder: SelectionHolder<TabSessionState>,
        interactor: TabsTrayInteractor,
    ) {
        // Since I immediately pass the event to onTouchEvent if it's not a move
        // The ClickableViewAccessibility warning isn't useful
        itemView.setOnTouchListener { view, motionEvent ->
            when (motionEvent.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    touchStartPoint = PointF(motionEvent.x, motionEvent.y)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    touchStartPoint = null
                }
                MotionEvent.ACTION_MOVE -> {
                    val touchStart = touchStartPoint
                    val selected = holder.selectedItems
                    val selectsOnlyThis = (selected.size == 1 && selected.contains(item))
                    if (selectsOnlyThis && touchStart != null) {
                        // If the parent is null then return early and mark the event as unhandled
                        val parent = itemView.parent as? AbstractBrowserTrayList ?: return@setOnTouchListener false

                        // Prevent scrolling if the user tries to start drag vertically
                        parent.requestDisallowInterceptTouchEvent(true)
                        // Only start deselect+drag if the user drags far enough
                        val dist = PointF.length(touchStart.x - motionEvent.x, touchStart.y - motionEvent.y)
                        if (dist > ViewConfiguration.get(parent.context).scaledTouchSlop) {
                            interactor.onTabUnselected(item) // Exit selection mode
                            touchStartPoint = null
                            val dragOffset = PointF(motionEvent.x, motionEvent.y)
                            val shadow = BlankDragShadowBuilder()
                            ViewCompat.startDragAndDrop(itemView, null, shadow, TabDragData(item, dragOffset), 0)
                        }
                        return@setOnTouchListener true
                    }
                }
            }
            view.onTouchEvent(motionEvent)
        }
    }

    companion object {
        internal const val PLAY_PAUSE_BUTTON_EXTRA_DPS = 24
        internal const val GRID_ITEM_CLOSE_BUTTON_EXTRA_DPS = 24
    }
}
