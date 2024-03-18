/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions.facts

import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Addons feature.
 */
class WebExtensionFacts {
    /**
     * Items that specify which portion of the web extension events were invoked.
     */
    object Items {
        const val WEB_EXTENSIONS_INITIALIZED = "web_extensions_initialized"
    }
}

private fun emitWebExtensionFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.SUPPORT_WEBEXTENSIONS,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitWebExtensionsInitializedFact(extensions: List<WebExtension>) {
    val installedAddons = extensions.filter { !it.isBuiltIn() }
    val enabledAddons = installedAddons.filter { it.isEnabled() }
    emitWebExtensionFact(
        Action.INTERACTION,
        WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED,
        metadata = mapOf(
            "installed" to installedAddons.map { it.id },
            "enabled" to enabledAddons.map { it.id },
        ),
    )
}
