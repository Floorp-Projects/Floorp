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
import android.view.MotionEvent
import android.view.View
import android.widget.ImageView
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.core.view.marginTop
import androidx.lifecycle.LifecycleOwner
import com.google.android.material.appbar.AppBarLayout
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.*
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.utils.Settings

/**
 * Displays an overlay above the tracking protection button in the browser toolbar
 * to onboard the user about tracking protection.
 */
@ExperimentalCoroutinesApi
class TrackingProtectionOverlay(
    private val context: Context,
    private val settings: Settings,
    private val metrics: MetricController,
    private val store: BrowserStore,
    private val lifecycleOwner: LifecycleOwner,
    private val getToolbar: () -> View
) : LifecycleAwareFeature {

    @VisibleForTesting
    internal var scope: CoroutineScope? = null

    override fun start() {
        store.flowScoped(lifecycleOwner) { flow ->
            flow.mapNotNull { state ->
                state.selectedTab
            }.ifChanged { tab ->
                tab.content.loading
            }
                .collect { tab ->
                    onLoadingStateChanged(tab)
                }
        }
    }

    override fun stop() {
        cancelScope()
    }

    @VisibleForTesting
    internal fun cancelScope() = scope?.cancel()

    @VisibleForTesting
    internal fun onLoadingStateChanged(tab: SessionState) {
        if (shouldShowTrackingProtectionOnboarding(tab) &&
            tab.content.progress == FULL_PROGRESS &&
            settings.shouldUseTrackingProtection
        ) {
            showTrackingProtectionOnboarding()
        }
    }

    private fun shouldShowTrackingProtectionOnboarding(tab: SessionState) =
        tab.trackingProtection.enabled &&
            tab.trackingProtection.blockedTrackers.isNotEmpty() &&
            settings.shouldShowTrackingProtectionCfr

    @Suppress("MagicNumber", "InflateParams")
    private fun showTrackingProtectionOnboarding() {

        if (!getToolbar().hasWindowFocus()) return

        val toolbarPosition = settings.toolbarPosition

        when (toolbarPosition) {
            ToolbarPosition.BOTTOM -> {
                if (getToolbar().translationY > 0) {
                    return
                }
            }
            ToolbarPosition.TOP -> {
                val appBarLayout = getToolbar().parent as? AppBarLayout
                appBarLayout?.let { appBar ->
                    if (appBar.y != 0.toFloat()) {
                        return
                    }
                }
            }
        }

        val trackingOnboardingDialog = object : Dialog(context) {
            override fun onTouchEvent(event: MotionEvent): Boolean {

                if (event.action == MotionEvent.ACTION_DOWN) {
                    metrics.track(Event.ContextualHintETPOutsideTap)
                }
                return super.onTouchEvent(event)
            }
        }

        val layout = LayoutInflater.from(context)
            .inflate(R.layout.tracking_protection_onboarding_popup, null)

        layout.drop_down_triangle.isVisible = toolbarPosition == ToolbarPosition.TOP
        layout.pop_up_triangle.isVisible = toolbarPosition == ToolbarPosition.BOTTOM

        layout.onboarding_message.text =
            context.getString(
                R.string.etp_onboarding_cfr_message,
                context.getString(R.string.app_name)
            )

        val closeButton = layout.findViewById<ImageView>(R.id.close_onboarding)
        closeButton.increaseTapArea(BUTTON_INCREASE_DPS)
        closeButton.setOnClickListener {
            metrics.track(Event.ContextualHintETPDismissed)
            trackingOnboardingDialog.dismiss()
        }

        val res = context.resources
        val triangleWidthPx = res.getDimension(R.dimen.cfr_triangle_height)
        val triangleMarginStartPx = res.getDimension(R.dimen.cfr_triangle_margin_edge)

        val toolbar = getToolbar()
        val trackingProtectionIcon: View =
            toolbar.findViewById(R.id.mozac_browser_toolbar_security_indicator)

        val xOffset = triangleMarginStartPx + triangleWidthPx / 2

        val gravity = Gravity.START or toolbarPosition.androidGravity

        trackingOnboardingDialog.apply {
            setContentView(layout)
            setCancelable(false)
            // removing title or setting it as an empty string does not prevent a11y services from assigning one
            setTitle(" ")
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

        trackingOnboardingDialog.message.setOnClickListener {
            metrics.track(Event.ContextualHintETPInsideTap)
            trackingOnboardingDialog.dismiss()
            trackingProtectionIcon.performClick()
        }

        metrics.track(Event.ContextualHintETPDisplayed)
        trackingOnboardingDialog.show()
        settings.lastCfrShownTimeInMillis = System.currentTimeMillis()
        settings.incrementTrackingProtectionOnboardingCount()
    }

    private companion object {
        private const val FULL_PROGRESS = 100
        private const val BUTTON_INCREASE_DPS = 12
    }
}
