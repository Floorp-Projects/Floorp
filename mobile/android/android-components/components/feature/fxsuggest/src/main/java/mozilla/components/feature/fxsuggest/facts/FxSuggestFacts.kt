/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest.facts

import mozilla.components.feature.fxsuggest.FxSuggestClickInfo
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Firefox Suggest feature.
 */
class FxSuggestFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val AMP_SUGGESTION_CLICKED = "amp_suggestion_clicked"
    }

    /**
     * Keys used in the metadata map.
     */
    object MetadataKeys {
        const val CLICK_INFO = "click_info"
    }
}

private fun emitFxSuggestFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_FXSUGGEST,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitSponsoredSuggestionClickedFact(clickInfo: FxSuggestClickInfo) {
    emitFxSuggestFact(
        Action.INTERACTION,
        FxSuggestFacts.Items.AMP_SUGGESTION_CLICKED,
        metadata = mapOf(FxSuggestFacts.MetadataKeys.CLICK_INFO to clickInfo),
    )
}
