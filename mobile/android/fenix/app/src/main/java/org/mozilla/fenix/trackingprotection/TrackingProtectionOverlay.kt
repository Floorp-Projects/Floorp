/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.core.view.marginTop
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.*
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
        if (!getToolbar().hasWindowFocus()) return
        val trackingOnboardingDialog = Dialog(context)
        val layout = LayoutInflater.from(context)
            .inflate(R.layout.tracking_protection_onboarding_popup, null)
        val isBottomToolbar = Settings.getInstance(context).shouldUseBottomToolbar

        layout.drop_down_triangle.isGone = isBottomToolbar
        layout.pop_up_triangle.isVisible = isBottomToolbar

        layout.onboarding_message.text =
            context.getString(
                R.string.etp_onboarding_cfr_message,
                context.getString(R.string.app_name)
            )

        val closeButton = layout.findViewById<ImageView>(R.id.close_onboarding)
        closeButton.increaseTapArea(BUTTON_INCREASE_DPS)
        closeButton.setOnClickListener {
            trackingOnboardingDialog.dismiss()
        }

        val res = context.resources
        val triangleWidthPx = res.getDimension(R.dimen.tp_onboarding_triangle_height)
        val triangleMarginStartPx = res.getDimension(R.dimen.tp_onboarding_triangle_margin_start)

        val toolbar = getToolbar()
        val trackingProtectionIcon: View =
            toolbar.findViewById(R.id.mozac_browser_toolbar_tracking_protection_indicator)

        val xOffset = triangleMarginStartPx + triangleWidthPx / 2

        val gravity = if (isBottomToolbar) {
            Gravity.START or Gravity.BOTTOM
        } else {
            Gravity.START or Gravity.TOP
        }

        trackingOnboardingDialog.apply {
            setContentView(layout)
            setCancelable(false)
        }

        trackingOnboardingDialog.window?.let {
            it.setGravity(gravity)
            val attr = it.attributes
            attr.x =
                (trackingProtectionIcon.x + trackingProtectionIcon.width / 2 - xOffset).toInt()
            attr.y =
                (trackingProtectionIcon.y + trackingProtectionIcon.height - trackingProtectionIcon.marginTop).toInt()
            it.attributes = attr
            it.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }

        val etpShield =
            getToolbar().findViewById<View>(R.id.mozac_browser_toolbar_tracking_protection_indicator)
        trackingOnboardingDialog.message.setOnClickListener {
            trackingOnboardingDialog.dismiss()
            etpShield.performClick()
        }

        trackingOnboardingDialog.show()
        settings.incrementTrackingProtectionOnboardingCount()
    }

    private companion object {
        private const val BUTTON_INCREASE_DPS = 12
    }
}
