/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.SessionState

class TelemetrySessionObserver : SessionManager.Observer {
    override fun onSessionAdded(session: Session) {
        when (session.source) {
            SessionState.Source.ACTION_VIEW -> TelemetryWrapper.browseIntentEvent()
            SessionState.Source.ACTION_SEND -> TelemetryWrapper.shareIntentEvent(session.searchTerms.isNotEmpty())
            SessionState.Source.TEXT_SELECTION -> TelemetryWrapper.textSelectionIntentEvent()
            SessionState.Source.HOME_SCREEN -> TelemetryWrapper.openHomescreenShortcutEvent()
            SessionState.Source.CUSTOM_TAB -> TelemetryWrapper.customTabsIntentEvent(
                    generateOptions(session.customTabConfig!!))
            else -> {
                // For other session types we create events at the place where we create the sessions.
            }
        }
    }

    /**
     * This method creates a list of options used to share with Telemetry and was migrated from A-C.
     *
     * @param customTabConfig The customTabConfig to use
     * @return A list of strings representing the customTabConfig
     */
    @Suppress("ComplexMethod")
    private fun generateOptions(customTabConfig: CustomTabConfig): List<String> {
        val options = mutableListOf<String>()

        if (customTabConfig.toolbarColor != null) options.add(TOOLBAR_COLOR_OPTION)
        if (customTabConfig.closeButtonIcon != null) options.add(CLOSE_BUTTON_OPTION)
        if (customTabConfig.enableUrlbarHiding) options.add(DISABLE_URLBAR_HIDING_OPTION)
        if (customTabConfig.actionButtonConfig != null) options.add(ACTION_BUTTON_OPTION)
        if (customTabConfig.showShareMenuItem) options.add(SHARE_MENU_ITEM_OPTION)
        if (customTabConfig.menuItems.isNotEmpty()) options.add(CUSTOMIZED_MENU_OPTION)
        if (customTabConfig.actionButtonConfig?.tint == true) options.add(ACTION_BUTTON_TINT_OPTION)
        if (customTabConfig.exitAnimations != null) options.add(EXIT_ANIMATION_OPTION)
        if (customTabConfig.titleVisible) options.add(PAGE_TITLE_OPTION)

        return options
    }

    companion object {
        internal const val TOOLBAR_COLOR_OPTION = "hasToolbarColor"
        internal const val CLOSE_BUTTON_OPTION = "hasCloseButton"
        internal const val DISABLE_URLBAR_HIDING_OPTION = "disablesUrlbarHiding"
        internal const val ACTION_BUTTON_OPTION = "hasActionButton"
        internal const val SHARE_MENU_ITEM_OPTION = "hasShareItem"
        internal const val CUSTOMIZED_MENU_OPTION = "hasCustomizedMenu"
        internal const val ACTION_BUTTON_TINT_OPTION = "hasActionButtonTint"
        internal const val EXIT_ANIMATION_OPTION = "hasExitAnimation"
        internal const val PAGE_TITLE_OPTION = "hasPageTitle"
    }
}
