/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

private fun emitCustomTabsFact(
    action: Action,
    item: String
) {
    Fact(
        Component.FEATURE_CUSTOMTABS,
        action,
        item
    ).collect()
}

private object CustomTabsItems {
    const val CLOSE = "close"
    const val ACTION_BUTTON = "action_button"
}

internal fun emitCloseFact() = emitCustomTabsFact(Action.CLICK, CustomTabsItems.CLOSE)
internal fun emitActionButtonFact() = emitCustomTabsFact(Action.CLICK, CustomTabsItems.ACTION_BUTTON)
