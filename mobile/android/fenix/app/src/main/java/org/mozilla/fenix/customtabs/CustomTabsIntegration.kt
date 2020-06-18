/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.app.Activity
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.ext.settings

class CustomTabsIntegration(
    sessionManager: SessionManager,
    toolbar: BrowserToolbar,
    sessionId: String,
    activity: Activity,
    onItemTapped: (ToolbarMenu.Item) -> Unit = {},
    shouldReverseItems: Boolean,
    isPrivate: Boolean
) : LifecycleAwareFeature, UserInteractionHandler {

    init {
        // Remove toolbar shadow
        toolbar.elevation = 0f

        val uncoloredEtpShield = AppCompatResources.getDrawable(
            activity,
            R.drawable.ic_tracking_protection_enabled
        )!!

        toolbar.display.icons = toolbar.display.icons.copy(
            // Custom private tab backgrounds have bad contrast against the colored shield
            trackingProtectionTrackersBlocked = uncoloredEtpShield,
            trackingProtectionNothingBlocked = uncoloredEtpShield,
            trackingProtectionException = AppCompatResources.getDrawable(
                activity,
                R.drawable.ic_tracking_protection_disabled
            )!!
        )

        toolbar.display.displayIndicatorSeparator = false
        if (activity.settings().shouldUseTrackingProtection) {
            toolbar.display.indicators = listOf(
                DisplayToolbar.Indicators.SECURITY,
                DisplayToolbar.Indicators.TRACKING_PROTECTION
            )
        } else {
            toolbar.display.indicators = listOf(
                DisplayToolbar.Indicators.SECURITY
            )
        }

        // If in private mode, override toolbar background to use private color
        // See #5334
        if (isPrivate) {
            sessionManager.findSessionById(sessionId)?.apply {
                val config = customTabConfig
                customTabConfig = config?.copy(
                    // Don't set toolbar background automatically
                    toolbarColor = null,
                    // Force tinting the action button
                    actionButtonConfig = config.actionButtonConfig?.copy(tint = true)
                )
            }

            toolbar.background = AppCompatResources.getDrawable(
                activity,
                R.drawable.toolbar_background
            )
        }
    }

    private val customTabToolbarMenu by lazy {
        CustomTabToolbarMenu(
            activity,
            sessionManager,
            sessionId,
            shouldReverseItems,
            onItemTapped = onItemTapped
        )
    }

    private val feature = CustomTabsToolbarFeature(
        sessionManager,
        toolbar,
        sessionId,
        menuBuilder = customTabToolbarMenu.menuBuilder,
        menuItemIndex = START_OF_MENU_ITEMS_INDEX,
        window = activity.window,
        shareListener = { onItemTapped.invoke(ToolbarMenu.Item.Share) },
        closeListener = { activity.finish() }
    )

    override fun start() = feature.start()
    override fun stop() = feature.stop()
    override fun onBackPressed() = feature.onBackPressed()

    companion object {
        private const val START_OF_MENU_ITEMS_INDEX = 2
    }
}
