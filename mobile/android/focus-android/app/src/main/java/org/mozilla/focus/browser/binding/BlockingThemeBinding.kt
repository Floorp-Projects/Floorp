/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import android.graphics.drawable.TransitionDrawable
import android.view.View
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.focus.R
import org.mozilla.focus.animation.TransitionDrawableGroup

private const val ANIMATION_DURATION = 300

/**
 * Binding responsible for updating the theme based on the loading state and whether
 */
class BlockingThemeBinding(
    store: BrowserStore,
    private val tabId: String,
    private val isCustomTab: Boolean,
    private val statusBar: View,
    private val urlBar: View
) : AbstractBinding<BrowserState>(store) {
    private var backgroundTransitionGroup: TransitionDrawableGroup? = updateResources(
        isCustomTab, enabled = true
    )
    private var hasLoadedOnce = false
    private var trackingProtectionDisabled = false

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .ifAnyChanged { tab -> arrayOf(tab.trackingProtection.ignoredOnTrackingProtection, tab.content.loading) }
            .collect { tab -> onLoadingStateChanged(tab) }
    }

    private fun onLoadingStateChanged(tab: SessionState) {
        if (tab.trackingProtection.ignoredOnTrackingProtection != trackingProtectionDisabled) {
            trackingProtectionDisabled = tab.trackingProtection.ignoredOnTrackingProtection
            backgroundTransitionGroup = updateResources(isCustomTab, trackingProtectionDisabled.not())
        }

        if (tab.content.loading) {
            backgroundTransitionGroup?.resetTransition()
        } else {
            // We start a transition animation only if a page was just loading before, avoiding issue #1179
            if (hasLoadedOnce) {
                backgroundTransitionGroup?.startTransition(ANIMATION_DURATION)
            } else {
                backgroundTransitionGroup?.transitionImmediately()
            }
            hasLoadedOnce = true
        }
    }

    private fun updateResources(
        isCustomTab: Boolean,
        enabled: Boolean
    ): TransitionDrawableGroup {
        statusBar.setBackgroundResource(
            if (enabled) {
                R.drawable.animated_background
            } else {
                R.drawable.animated_background_disabled
            }
        )

        return if (!isCustomTab) {
            // Only update the toolbar background if this is not a custom tab. Custom tabs set their
            // own color and we do not want to override this here.
            urlBar.setBackgroundResource(
                if (enabled) {
                    R.drawable.animated_background
                } else {
                    R.drawable.animated_background_disabled
                }
            )

            TransitionDrawableGroup(
                urlBar.background as TransitionDrawable,
                statusBar.background as TransitionDrawable
            )
        } else {
            TransitionDrawableGroup(
                statusBar.background as TransitionDrawable
            )
        }
    }
}
