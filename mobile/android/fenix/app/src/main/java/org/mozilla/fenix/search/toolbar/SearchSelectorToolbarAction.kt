/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.concept.menu.Orientation
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.toScope
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.SearchDialogFragmentStore

/**
 * A [Toolbar.Action] implementation that shows a [SearchSelector].
 *
 * @property store [SearchDialogFragmentStore] containing the complete state of the search dialog.
 * @property menu An instance of [SearchSelectorMenu] to display a popup menu for the search
 * selections.
 */
class SearchSelectorToolbarAction(
    private val store: SearchDialogFragmentStore,
    private val defaultSearchEngine: SearchEngine?,
    private val menu: SearchSelectorMenu,
) : Toolbar.Action {
    private var updateIconJob: Job? = null

    override fun createView(parent: ViewGroup): View {
        val context = parent.context

        // Only search engines with type APPLICATION (tabs, history, bookmarks) will have a valid icon at this time.
        // For other search engines show the icon of the default search engine which should be shown in the selector.
        val initialSearchEngine = store.state.searchEngineSource.searchEngine ?: defaultSearchEngine
        return SearchSelector(context).apply {
            initialSearchEngine?.let {
                this.setIcon(
                    icon = initialSearchEngine.getScaledIcon(this.context),
                    contentDescription = initialSearchEngine.name,
                )
            }

            setOnClickListener {
                val orientation = if (context.settings().shouldUseBottomToolbar) {
                    Orientation.UP
                } else {
                    Orientation.DOWN
                }

                UnifiedSearch.searchMenuTapped.record(NoExtras())
                menu.menuController.show(
                    anchor = it.findViewById(R.id.search_selector),
                    orientation = orientation,
                )
            }

            val topPadding = resources.getDimensionPixelSize(R.dimen.search_engine_engine_icon_top_margin)
            setPadding(0, topPadding, 0, 0)

            setBackgroundResource(
                context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless),
            )
        }
    }

    override fun bind(view: View) {
        // It may happen that this View is binded multiple times.
        // Prevent launching new coroutines for every time this is binded and only update the icon once.
        if (updateIconJob?.isActive != true) {
            updateIconJob = (view as? SearchSelector)?.toScope()?.launch {
                store.flow()
                    .map { state -> state.searchEngineSource.searchEngine }
                    .filterNotNull()
                    .distinctUntilChanged()
                    .collect { searchEngine ->
                        view.setIcon(
                            icon = searchEngine.getScaledIcon(view.context).apply {
                                // Setting tint manually for icons that were converted from Drawable
                                // to Bitmap. Search Engine icons are stored as Bitmaps, hence
                                // theming/attribute mechanism won't work.
                                if (searchEngine.type == SearchEngine.Type.APPLICATION) {
                                    setTint(view.context.getColorFromAttr(R.attr.textPrimary))
                                }
                            },
                            contentDescription = searchEngine.name,
                        )
                    }
            }.also {
                it?.start()
            }
        }
    }
}

/**
 * Get the search engine icon appropriately scaled to be shown in the selector.
 */
@VisibleForTesting
internal fun SearchEngine.getScaledIcon(context: Context): BitmapDrawable {
    val iconSize =
        context.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)
    val scaledIcon = Bitmap.createScaledBitmap(
        icon,
        iconSize,
        iconSize,
        true,
    )

    return BitmapDrawable(context.resources, scaledIcon)
}
