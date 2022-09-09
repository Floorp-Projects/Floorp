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
import org.mozilla.fenix.GleanMetrics.RecentTabs
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.cfr.CFRPopup
import org.mozilla.fenix.compose.cfr.CFRPopupProperties
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTabViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabsHeaderViewHolder

/**
 * Vertical padding needed to improve the visual alignment of the popup and respect the UX design.
 */
private const val CFR_TO_ANCHOR_VERTICAL_PADDING = -16

/**
 * Delegate for handling the Home Onboarding CFR.
 *
 * @param context [Context] used for various Android interactions.
 * @param recyclerView [RecyclerView] will serve as anchor for the CFR.
 */
class HomeCFRPresenter(
    private val context: Context,
    private val recyclerView: RecyclerView,
) {

    /**
     * Determine the CFR to be shown on the Home screen and show a CFR for the resultant view
     * if any.
     */
    fun show() {
        when (val result = getCFRToShow()) {
            is Result.SyncedTab -> showSyncedTabCFR(view = result.view)
            is Result.JumpBackIn -> showJumpBackInCFR(view = result.view)
            else -> {
                // no-op
            }
        }
    }

    private fun showSyncedTabCFR(view: View) {
        CFRPopup(
            text = context.getString(R.string.sync_cfr_message),
            anchor = view,
            properties = CFRPopupProperties(
                indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
                popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
            ),
            onDismiss = {
                when (it) {
                    true -> Onboarding.syncCfrExplicitDismissal.record(NoExtras())
                    false -> Onboarding.syncCfrImplicitDismissal.record(NoExtras())
                }
            },
        ).show()

        // Turn off both the recent tab and synced tab CFR after the recent synced tab CFR is shown.
        context.settings().showSyncCFR = false
        context.settings().shouldShowJumpBackInCFR = false

        Onboarding.synCfrShown.record(NoExtras())
    }

    @Suppress("MagicNumber")
    private fun showJumpBackInCFR(view: View) {
        CFRPopup(
            text = context.getString(R.string.onboarding_home_screen_jump_back_contextual_hint_2),
            anchor = view,
            properties = CFRPopupProperties(
                indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
                popupVerticalOffset = (-40).dp, // Offset the top spacer in the recent tabs header.
            ),
            onDismiss = {
                when (it) {
                    true -> RecentTabs.jumpBackInCfrDismissed.record(NoExtras())
                    false -> RecentTabs.jumpBackInCfrCancelled.record(NoExtras())
                }
            },
        ).show()

        // Users can still see the recent synced tab CFR after the recent tab CFR is shown in
        // subsequent navigation to the Home screen.
        context.settings().shouldShowJumpBackInCFR = false

        RecentTabs.jumpBackInCfrShown.record(NoExtras())
    }

    /**
     * Returns a [Result] that indicates the CFR that should be shown on the Home screen if any
     * based on the views available and the preferences.
     */
    private fun getCFRToShow(): Result {
        var result: Result = Result.None
        val count = recyclerView.adapter?.itemCount ?: return result

        for (index in count downTo 0) {
            val viewHolder = recyclerView.findViewHolderForAdapterPosition(index)

            if (context.settings().showSyncCFR && viewHolder is RecentSyncedTabViewHolder) {
                result = Result.SyncedTab(view = viewHolder.composeView)
                break
            } else if (context.settings().shouldShowJumpBackInCFR &&
                viewHolder is RecentTabsHeaderViewHolder
            ) {
                result = Result.JumpBackIn(view = viewHolder.composeView)
            }
        }

        return result
    }

    /**
     * The result of determining which CFR to show on the Home screen.
     */
    sealed class Result {
        /**
         * Indicates no CFR should be shown on the Home screen.
         */
        object None : Result()

        /**
         * Indicates a CFR should be shown for a Synced Tab and the associated [view] to anchor
         * the CFR.
         */
        data class SyncedTab(val view: View) : Result()

        /**
         * Indicates a CFR should be for Jump Back In and the associated [view] to anchor the CFR.
         */
        data class JumpBackIn(val view: View) : Result()
    }
}
