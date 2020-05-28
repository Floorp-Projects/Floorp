/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.cfr

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.core.view.marginTop
import kotlinx.android.synthetic.main.search_widget_cfr.view.*
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.drop_down_triangle
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.pop_up_triangle
import org.mozilla.fenix.R
import org.mozilla.fenix.components.SearchWidgetCreator
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

/**
 * Displays a CFR above the HomeFragment toolbar that recommends usage / installation of the search widget.
 */
class SearchWidgetCFR(
    private val context: Context,
    private val getToolbar: () -> View
) {

    fun displayIfNecessary() {
        if (!context.settings().isInSearchWidgetExperiment ||
            !context.settings().shouldDisplaySearchWidgetCFR()) { return }
        showSearchWidgetCFR()
    }

    @Suppress("MagicNumber", "InflateParams")
    private fun showSearchWidgetCFR() {
        context.settings().incrementSearchWidgetCFRDisplayed()

        val searchWidgetCFRDialog = Dialog(context)
        val layout = LayoutInflater.from(context)
            .inflate(R.layout.search_widget_cfr, null)
        val isBottomToolbar = Settings.getInstance(context).shouldUseBottomToolbar

        layout.drop_down_triangle.isGone = isBottomToolbar
        layout.pop_up_triangle.isVisible = isBottomToolbar

        val toolbar = getToolbar()

        val gravity = if (isBottomToolbar) {
            Gravity.CENTER_HORIZONTAL or Gravity.BOTTOM
        } else {
            Gravity.CENTER_HORIZONTAL or Gravity.TOP
        }

        layout.cfr_neg_button.setOnClickListener {
            context.components.analytics.metrics.track(Event.SearchWidgetCFRNotNowPressed)
            searchWidgetCFRDialog.dismiss()
            context.settings().manuallyDismissSearchWidgetCFR()
        }

        layout.cfr_pos_button.setOnClickListener {
            context.components.analytics.metrics.track(Event.SearchWidgetCFRAddWidgetPressed)
            SearchWidgetCreator.createSearchWidget(context)
            searchWidgetCFRDialog.dismiss()
            context.settings().manuallyDismissSearchWidgetCFR()
        }

        searchWidgetCFRDialog.apply {
            setContentView(layout)
        }

        searchWidgetCFRDialog.window?.let {
            it.setGravity(gravity)
            val attr = it.attributes
            attr.y =
                (toolbar.y + toolbar.height - toolbar.marginTop - toolbar.paddingTop).toInt()
            it.attributes = attr
            it.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }

        searchWidgetCFRDialog.setOnCancelListener {
            context.components.analytics.metrics.track(Event.SearchWidgetCFRCanceled)
        }

        searchWidgetCFRDialog.setOnDismissListener {
            context.settings().incrementSearchWidgetCFRDismissed()
        }

        searchWidgetCFRDialog.show()
        context.components.analytics.metrics.track(Event.SearchWidgetCFRDisplayed)
    }
}
