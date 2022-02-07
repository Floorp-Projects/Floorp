/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.util.TypedValue
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.PopupWindow
import org.mozilla.focus.R
import org.mozilla.focus.databinding.ToolbarShieldIconCfrBinding
import org.mozilla.focus.ext.settings
import kotlin.math.roundToInt

class CfrUtils {
    data class CFRForShieldToolbarIcon(
        val toolbarShieldIconCfrBinding: ToolbarShieldIconCfrBinding,
        val toolbarShieldIconCfrPopupWindow: PopupWindow
    )

    companion object {
        private const val SHIELD_ICON_CFR_Y_OFFSET = -18f

        /**
         * Shows CFR for Shield Toolbar Icon  if content is secure.
         * This should be shown only when Shield Icon is visible.
         *
         * @property rootView of Browser Toolbar.
         * @property context where CFR should be shown.
         * @property isContentSecure true if the tab is currently pointed to a URL
         * with a valid SSL certificate, otherwise false.
         */
        fun showCFRForShieldToolbarIconIfNeeded(
            rootView: View,
            context: Context,
            isContentSecure: Boolean
        ): CFRForShieldToolbarIcon? {
            val shieldToolbarIcon = rootView.findViewById<View>(
                R.id.mozac_browser_toolbar_tracking_protection_indicator
            )
            if (shouldShowCFRForShieldToolbarIcon(
                    shieldToolbarIcon = shieldToolbarIcon, context = context,
                    isContentSecure = isContentSecure
                )
            ) {

                val toolbarShieldIconCfrBinding = ToolbarShieldIconCfrBinding.inflate(
                    LayoutInflater.from(
                        context
                    )
                )
                val toolbarShieldIconCfrPopupWindow =
                    PopupWindow(
                        toolbarShieldIconCfrBinding.root,
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        true
                    )
                toolbarShieldIconCfrPopupWindow.showAsDropDown(
                    shieldToolbarIcon, 0,
                    TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP, SHIELD_ICON_CFR_Y_OFFSET,
                        context.resources.displayMetrics
                    ).roundToInt(),
                    Gravity.BOTTOM
                )
                toolbarShieldIconCfrBinding.closeInfoBanner.setOnClickListener {
                    toolbarShieldIconCfrPopupWindow.dismiss()
                }
                toolbarShieldIconCfrPopupWindow.setOnDismissListener {
                    context.settings.shouldShowCfrForShieldToolbarIcon = false
                }
                return CFRForShieldToolbarIcon(toolbarShieldIconCfrBinding, toolbarShieldIconCfrPopupWindow)
            }
            return null
        }

        private fun shouldShowCFRForShieldToolbarIcon(
            shieldToolbarIcon: View?,
            context: Context,
            isContentSecure: Boolean
        ): Boolean =
            Features.SHOULD_SHOW_CFR_FOR_SHIELD_TOOLBAR_ICON && shieldToolbarIcon != null &&
                context.settings.shouldShowCfrForShieldToolbarIcon &&
                isContentSecure
    }
}
