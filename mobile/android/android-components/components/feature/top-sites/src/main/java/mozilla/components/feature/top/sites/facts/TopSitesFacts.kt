/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [TopSitesFeature]
 */
class TopSitesFacts {
    /**
     * Items that specify which portion of the [TopSitesFeature] was interacted with
     */
    object Items {
        const val COUNT = "count"
    }
}

internal fun emitTopSitesCountFact(count: Int) {
    Fact(
        Component.FEATURE_TOP_SITES,
        Action.INTERACTION,
        TopSitesFacts.Items.COUNT,
        count.toString(),
    ).collect()
}
