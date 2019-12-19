/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.View.MeasureSpec
import android.view.WindowManager
import android.widget.ImageView
import android.widget.PopupWindow
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.*
import mozilla.components.browser.session.Session
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.utils.Settings

/**
 * Displays an overlay above the tracking protection button in the browser toolbar
 * to onboard the user about tracking protection.
 */
class TrackingProtectionOverlay(
    private val context: Context,
    private val settings: Settings,
    private val getToolbar: () -> View
) : Session.Observer {

    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        if (!loading && shouldShowTrackingProtectionOnboarding(session)) {
            showTrackingProtectionOnboarding()
        }
    }

    private fun shouldShowTrackingProtectionOnboarding(session: Session) =
        settings.shouldShowTrackingProtectionOnboarding &&
                session.trackerBlockingEnabled &&
                session.trackersBlocked.isNotEmpty()

    @Suppress("MagicNumber", "InflateParams")
    private fun showTrackingProtectionOnboarding() {
        val layout = LayoutInflater.from(context)
            .inflate(R.layout.tracking_protection_onboarding_popup, null)
        layout.onboarding_message.text =
            context.getString(R.string.etp_onboarding_message_2, context.getString(R.string.app_name))

        val res = context.resources
        val trackingOnboarding = PopupWindow(
            layout,
            res.getDimensionPixelSize(R.dimen.tp_onboarding_width),
            WindowManager.LayoutParams.WRAP_CONTENT
        ).apply {
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            isOutsideTouchable = true
            isFocusable = true
            elevation = res.getDimension(R.dimen.mozac_browser_menu_elevation)
            animationStyle = R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom
        }

        val closeButton = layout.findViewById<ImageView>(R.id.close_onboarding)
        closeButton.increaseTapArea(BUTTON_INCREASE_DPS)
        closeButton.setOnClickListener {
            trackingOnboarding.dismiss()
        }

        // Measure layout view
        val spec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
        layout.measure(spec, spec)

        val containerHeight = layout.measuredHeight
        val triangleHeight =
            (res.getDimension(R.dimen.tp_onboarding_triangle_height) / res.displayMetrics.density).toInt()

        val toolbar = getToolbar()
        val trackingProtectionIcon: View =
            toolbar.findViewById(R.id.mozac_browser_toolbar_tracking_protection_indicator)

        val xOffset = res.getDimensionPixelSize(R.dimen.tp_onboarding_x_offset)

        // Positioning the popup above the tp anchor.
        val yOffset = -containerHeight - (toolbar.height / 3 * 2) + triangleHeight

        trackingOnboarding.showAsDropDown(trackingProtectionIcon, xOffset, yOffset)
        settings.incrementTrackingProtectionOnboardingCount()
    }

    private companion object {
        private const val BUTTON_INCREASE_DPS = 12
    }
}
