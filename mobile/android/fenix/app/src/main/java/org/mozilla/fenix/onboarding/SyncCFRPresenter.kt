/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.view.View
import androidx.compose.ui.unit.dp
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.cfr.CFRPopup
import org.mozilla.fenix.compose.cfr.CFRPopupProperties
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTabViewHolder

/**
 * Vertical padding needed to improve the visual alignment of the popup and respect the UX design.
 */
private const val CFR_TO_ANCHOR_VERTICAL_PADDING = -16

/**
 * Delegate for handling sync onboarding CFR.
 *
 * @param [Context] used for various Android interactions.
 * @param [RecyclerView] will serve as anchor for the sync CFR.
 */
class SyncCFRPresenter(
    private val context: Context,
    private val recyclerView: RecyclerView,
) {

    private var syncCFR: CFRPopup? = null

    /**
     * Check if [view] is available to show sync CFR.
     */
    fun showSyncCFR() {
        findSyncTabsView()?.let {
            CFRPopup(
                text = context.getString(R.string.sync_cfr_message),
                anchor = it,
                properties = CFRPopupProperties(
                    indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
                    popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
                ),
                onDismiss = {
                    when (it) {
                        true -> Onboarding.syncCfrExplicitDismissal.record(NoExtras())
                        false -> Onboarding.syncCfrImplicitDismissal.record(NoExtras())
                    }
                }
            ) {
            }.apply {
                syncCFR = this
                show()
                Onboarding.synCfrShown.record(NoExtras())
            }
        }
    }

    private fun findSyncTabsView(): View? {
        val count = recyclerView.adapter?.itemCount ?: return null

        for (index in count downTo 0) {
            val viewHolder = recyclerView.findViewHolderForAdapterPosition(index)
            if (viewHolder is RecentSyncedTabViewHolder) {
                return viewHolder.composeView
            }
        }

        return null
    }
}
