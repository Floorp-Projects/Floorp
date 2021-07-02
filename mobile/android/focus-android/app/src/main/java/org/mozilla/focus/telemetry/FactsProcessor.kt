/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import androidx.annotation.VisibleForTesting
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts

/**
 * Processes all [Fact]s emitted from Android Components based on which the appropriate telemetry
 * will be collected.
 */
object FactsProcessor {
    fun initialize() {
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                fact.process()
            }
        })
    }

    @VisibleForTesting
    internal fun Fact.process() = when (Pair(component, item)) {
        Component.FEATURE_SEARCH to AdsTelemetry.SERP_ADD_CLICKED -> {
            TelemetryWrapper.clickAddInSearchEvent(value!!)
        }
        Component.FEATURE_SEARCH to AdsTelemetry.SERP_SHOWN_WITH_ADDS -> {
            TelemetryWrapper.searchWithAdsShownEvent(value!!)
        }
        Component.FEATURE_SEARCH to InContentTelemetry.IN_CONTENT_SEARCH -> {
            TelemetryWrapper.inContentSearchEvent(value!!)
        }
        else -> Unit
    }
}
