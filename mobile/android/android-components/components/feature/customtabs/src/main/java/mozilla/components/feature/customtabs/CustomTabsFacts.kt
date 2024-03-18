/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [CustomTabsToolbarFeature]
 */
class CustomTabsFacts {
    /**
     * Items that specify which portion of the [CustomTabsToolbarFeature] was interacted with
     */
    object Items {
        const val CLOSE = "close"
        const val ACTION_BUTTON = "action_button"
    }
}

private fun emitCustomTabsFact(
    action: Action,
    item: String,
) {
    Fact(
        Component.FEATURE_CUSTOMTABS,
        action,
        item,
    ).collect()
}

internal fun emitCloseFact() = emitCustomTabsFact(Action.CLICK, CustomTabsFacts.Items.CLOSE)
internal fun emitActionButtonFact() = emitCustomTabsFact(Action.CLICK, CustomTabsFacts.Items.ACTION_BUTTON)
