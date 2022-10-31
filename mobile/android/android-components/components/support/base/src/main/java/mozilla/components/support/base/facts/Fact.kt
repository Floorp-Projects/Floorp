/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.facts

import mozilla.components.support.base.Component

/**
 * A fact describing a generic event that has occurred in a component.
 *
 * @property component Component that emitted this fact.
 * @property action A user or system action that caused this fact (e.g. Action.CLICK).
 * @property item An item that caused the action or that the action was performed on (e.g. "toolbar").
 * @property value An optional value providing more context.
 * @property metadata A key/value map for facts where additional richer context is needed.
 */
data class Fact(
    val component: Component,
    val action: Action,
    val item: String,
    val value: String? = null,
    val metadata: Map<String, Any>? = null,
)

/**
 * Collect this fact through the [Facts] singleton.
 */
fun Fact.collect() = Facts.collect(this)
