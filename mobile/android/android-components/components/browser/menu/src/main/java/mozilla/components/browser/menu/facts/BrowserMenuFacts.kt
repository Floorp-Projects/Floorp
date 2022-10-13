/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [BrowserMenu].
 */
class BrowserMenuFacts {
    /**
     * Items that specify which portion of the [BrowserMenu] was interacted with.
     */
    object Items {
        const val WEB_EXTENSION_MENU_ITEM = "web_extension_menu_item"
    }
}

private fun emitMenuFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.BROWSER_MENU,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitOpenMenuItemFact(extensionId: String) {
    emitMenuFact(
        Action.CLICK,
        BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM,
        metadata = mapOf("id" to extensionId),
    )
}
