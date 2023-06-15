/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.content.ContextCompat
import androidx.core.view.updateLayoutParams
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

private const val NORMAL_HANDLE_PERCENT_WIDTH = 0.1F

/**
 * Various layout updates that need to be applied to the "handle" view when switching
 * between [Mode].
 *
 * @param store The TabsTrayStore instance.
 * @property handle The "handle" of the Tabs Tray that is used to drag the tray open/close.
 * @property containerLayout The [ConstraintLayout] that contains the "handle".
 */
@OptIn(ExperimentalCoroutinesApi::class)
class SelectionHandleBinding(
    store: TabsTrayStore,
    private val handle: View,
    private val containerLayout: ConstraintLayout,
) : AbstractBinding<TabsTrayState>(store) {

    private var isPreviousModeSelect = false

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.mode }
            .distinctUntilChanged()
            .collect { mode ->
                val isSelectMode = mode is Mode.Select

                // memoize to avoid unnecessary layout updates.
                if (isPreviousModeSelect != isSelectMode) {
                    updateLayoutParams(handle, isSelectMode)

                    updateBackgroundColor(handle, isSelectMode)

                    updateWidthPercent(containerLayout, handle, isSelectMode)
                }

                isPreviousModeSelect = isSelectMode
            }
    }

    private fun updateLayoutParams(handle: View, multiselect: Boolean) {
        handle.updateLayoutParams<ViewGroup.MarginLayoutParams> {
            height = handle.resources.getDimensionPixelSize(
                if (multiselect) {
                    R.dimen.tab_tray_multiselect_handle_height
                } else {
                    R.dimen.bottom_sheet_handle_height
                },
            )
            topMargin = handle.resources.getDimensionPixelSize(
                if (multiselect) {
                    R.dimen.tab_tray_multiselect_handle_top_margin
                } else {
                    R.dimen.bottom_sheet_handle_top_margin
                },
            )
        }
    }

    private fun updateBackgroundColor(handle: View, multiselect: Boolean) {
        val colorResource = if (multiselect) {
            R.color.fx_mobile_layer_color_accent
        } else {
            R.color.fx_mobile_text_color_secondary
        }

        val color = ContextCompat.getColor(handle.context, colorResource)

        handle.setBackgroundColor(color)
    }

    private fun updateWidthPercent(
        container: ConstraintLayout,
        handle: View,
        multiselect: Boolean,
    ) {
        val widthPercent = if (multiselect) 1F else NORMAL_HANDLE_PERCENT_WIDTH
        container.run {
            ConstraintSet().apply {
                clone(this@run)
                constrainPercentWidth(handle.id, widthPercent)
                applyTo(this@run)
            }
        }
    }
}
