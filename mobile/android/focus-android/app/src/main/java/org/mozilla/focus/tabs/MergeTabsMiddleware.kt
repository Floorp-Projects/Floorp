/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tabs

import android.content.Context
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.nimbus.FocusNimbus

/**
 * If the tabs feature is disabled then this middleware will look at incoming [TabListAction.AddTabAction]
 * actions and, instead of creating a new tab, will merge the new tab with the existing tab to create
 * a single tab with a merged state.
 */
class MergeTabsMiddleware(
    private val appContext: Context
) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        val multiTabsFeature = FocusNimbus.features.tabs
        if (multiTabsFeature.value(appContext).isMultiTab || action !is TabListAction.AddTabAction) {
            // If the experiment for tabs is enabled then we can just let the reducer create a
            // new tab.
            next(action)
            return
        }

        if (context.state.privateTabs.isEmpty()) {
            // If we do not have any tabs yet then we can let the reducer create one.
            next(action)
            return
        }

        val currentTab = context.state.privateTabs.first()
        val newTab = action.tab

        if (!shouldLoadInExistingTab(newTab.content.url) && !isFirstNewWindowRequest(currentTab, newTab)) {
            // This is a URL we do not want to load in an existing tab. Let's just bail.
            return
        }

        multiTabsFeature.recordExposure()
        val mergedTab = mergeTabs(currentTab, newTab)

        // First we add the merged tab. The engine middleware will take care of linking the existing
        // engine session to this tab.
        next(TabListAction.AddTabAction(mergedTab, select = true))

        // If the new tab does not have an engine session we will reuse the engine session of
        // the current tab so we unlink it first to prevent the engine middleware from closing it.
        if (newTab.engineState.engineSession == null) {
            context.dispatch(EngineAction.UnlinkEngineSessionAction(currentTab.id))
        }

        // If the new tab has an engine session (e.g. when it was opened by a web extension) we will
        // use the new engine session, close the current tab with its engine session.
        context.dispatch(TabListAction.RemoveTabAction(currentTab.id))

        // Now we load the URL in the new tab.
        // If we are to merge a window request we need to use that request's url coming from GeckoView
        // otherwise default to the new tab's url.
        val urlToLoad = currentTab.content.windowRequest?.url ?: newTab.content.url
        context.dispatch(
            EngineAction.LoadUrlAction(
                mergedTab.id,
                url = urlToLoad,
                flags = EngineSession.LoadUrlFlags.select(
                    // To be safe we use the external flag here, since its not the user who decided to
                    // load this URL in this existing session.
                    EngineSession.LoadUrlFlags.EXTERNAL
                )
            )
        )
    }
}

private fun mergeTabs(
    currentTab: TabSessionState,
    newTab: TabSessionState
): TabSessionState {
    // In case a new tab is being opened by a web extension, the new tab will have its own new engine/gecko session,
    // which will have to be used
    val newEngineState = if (newTab.engineState.engineSession != null) {
        newTab.engineState
    } else {
        currentTab.engineState.copy(
            // We are clearing the engine observer, which would update the state of the tab with the
            // old ID. The engine middleware will create a new observer.
            engineObserver = null
        )
    }

    // We are giving the ID of the new tab. This will make
    // sure that code that created the tab can still access it with the ID.
    return currentTab.copy(
        newTab.id,
        engineState = newEngineState,
        content = currentTab.content.copy(windowRequest = null)
    )
}

private fun shouldLoadInExistingTab(url: String): Boolean {
    val cleanedUrl = url.lowercase().trim()
    return cleanedUrl.startsWith("http:") ||
        cleanedUrl.startsWith("https:") ||
        cleanedUrl.startsWith("data:") ||
        cleanedUrl.startsWith("focus:")
}

/**
 * A new window request (target="_blank" links or window.open) will have it's own EngineSession
 * and there might be multiple AddAddTabAction("about:blank") about it.
 *
 * This method checks the status of both [currentTab] and [newTab] to ensure that we can only act once
 * at the right moment if needing to merge a new window request.
 */
private fun isFirstNewWindowRequest(currentTab: TabSessionState, newTab: TabSessionState): Boolean {
    val isNewWindowRequest = newTab.content.url == "about:blank" && newTab.engineState.engineSession != null
    val wasRequestConsumed = currentTab.content.windowRequest == null

    return isNewWindowRequest && !wasRequestConsumed
}
