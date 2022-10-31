/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View
import android.widget.PopupWindow
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.menu.facts.emitOpenMenuItemFact
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * A [BrowserMenu] capable of displaying browser and page actions from web extensions.
 */
class WebExtensionBrowserMenu internal constructor(
    adapter: BrowserMenuAdapter,
    private val store: BrowserStore,
) : BrowserMenu(adapter) {
    private var scope: CoroutineScope? = null

    override fun show(
        anchor: View,
        orientation: Orientation,
        style: MenuStyle?,
        endOfMenuAlwaysVisible: Boolean,
        onDismiss: () -> Unit,
    ): PopupWindow {
        scope = store.flowScoped { flow ->
            flow.ifChanged { it.selectedTab }
                .collect { state ->
                    getOrUpdateWebExtensionMenuItems(state, state.selectedTab)
                    invalidate()
                }
        }

        return super.show(
            anchor,
            orientation,
            style,
            endOfMenuAlwaysVisible,
            onDismiss,
        ).apply {
            setOnDismissListener {
                adapter.menu = null
                currentPopup = null
                scope?.cancel()
                webExtensionBrowserActions.clear()
                webExtensionPageActions.clear()
                onDismiss()
            }
        }
    }

    companion object {
        internal val webExtensionBrowserActions = HashMap<String, WebExtensionBrowserMenuItem>()
        internal val webExtensionPageActions = HashMap<String, WebExtensionBrowserMenuItem>()

        internal fun getOrUpdateWebExtensionMenuItems(
            state: BrowserState,
            tab: SessionState? = null,
        ): List<WebExtensionBrowserMenuItem> {
            val menuItems = ArrayList<WebExtensionBrowserMenuItem>()
            val extensions = state.extensions.values.toList()
            extensions.filter { it.enabled }.sortedBy { it.name }
                .forEach { extension ->
                    if (!extension.allowedInPrivateBrowsing && tab?.content?.private == true) {
                        return@forEach
                    }

                    extension.browserAction?.let { browserAction ->
                        addOrUpdateAction(
                            extension = extension,
                            globalAction = browserAction,
                            tabAction = tab?.extensionState?.get(extension.id)?.browserAction,
                            menuItems = menuItems,
                        )
                    }

                    extension.pageAction?.let { pageAction ->
                        val tabPageAction = tab?.extensionState?.get(extension.id)?.pageAction

                        // Unlike browser actions, page actions are not displayed by default (only if enabled):
                        // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action
                        if (pageAction.copyWithOverride(tabPageAction).enabled == true) {
                            addOrUpdateAction(
                                extension = extension,
                                globalAction = pageAction,
                                tabAction = tabPageAction,
                                menuItems = menuItems,
                                isPageAction = true,
                            )
                        }
                    }
                }

            return menuItems
        }

        private fun addOrUpdateAction(
            extension: WebExtensionState,
            globalAction: Action,
            tabAction: Action?,
            menuItems: ArrayList<WebExtensionBrowserMenuItem>,
            isPageAction: Boolean = false,
        ): Boolean {
            val actionMap = if (isPageAction) webExtensionPageActions else webExtensionBrowserActions

            // Add the global browser/page action if it doesn't exist
            val browserMenuItem = actionMap.getOrPut(extension.id) {
                val listener = {
                    emitOpenMenuItemFact(extension.id)
                    globalAction.onClick()
                }
                val browserMenuItem = WebExtensionBrowserMenuItem(
                    action = globalAction,
                    listener = listener,
                    id = extension.id,
                )
                browserMenuItem
            }

            // Apply tab-specific override of browser/page action
            tabAction?.let {
                browserMenuItem.action = globalAction.copyWithOverride(it)
            }

            return menuItems.add(browserMenuItem)
        }
    }
}
