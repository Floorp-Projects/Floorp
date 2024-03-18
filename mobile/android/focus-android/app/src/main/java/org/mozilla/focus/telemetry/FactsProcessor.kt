/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.facts.BrowserMenuFacts
import mozilla.components.feature.contextmenu.facts.ContextMenuFacts
import mozilla.components.feature.customtabs.CustomTabsFacts
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import org.mozilla.focus.GleanMetrics.Browser
import org.mozilla.focus.GleanMetrics.BrowserSearch
import org.mozilla.focus.GleanMetrics.ContextMenu
import org.mozilla.focus.GleanMetrics.CustomTabsToolbar

/**
 * Processes all [Fact]s emitted from Android Components based on which the appropriate telemetry
 * will be collected.
 */
object FactsProcessor {
    fun initialize() {
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    fact.process()
                }
            },
        )
    }

    @VisibleForTesting
    internal fun Fact.process() = when (Pair(component, item)) {
        Component.FEATURE_SEARCH to AdsTelemetry.SERP_ADD_CLICKED -> {
            BrowserSearch.adClicks[value!!].add()
        }
        Component.FEATURE_SEARCH to AdsTelemetry.SERP_SHOWN_WITH_ADDS -> {
            BrowserSearch.withAds[value!!].add()
        }
        Component.FEATURE_SEARCH to InContentTelemetry.IN_CONTENT_SEARCH -> {
            BrowserSearch.inContent[value!!].add()
        }

        Component.FEATURE_CUSTOMTABS to CustomTabsFacts.Items.CLOSE -> {
            CustomTabsToolbar.closeTabTapped.record(NoExtras())
        }

        Component.FEATURE_CUSTOMTABS to CustomTabsFacts.Items.ACTION_BUTTON -> {
            CustomTabsToolbar.actionButtonTapped.record(NoExtras())
        }

        Component.FEATURE_CONTEXTMENU to ContextMenuFacts.Items.ITEM -> {
            ContextMenu.itemTapped.record(ContextMenu.ItemTappedExtra(toContextMenuExtraKey()))
        }

        Component.BROWSER_MENU to BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM -> {
            if (metadata?.get("id") == "webcompat-reporter@mozilla.org") {
                Browser.reportSiteIssueCounter.add()
            } else {
                // other extension action was emitted
            }
        }

        else -> Unit
    }
}

/**
 * Extracts an extraKey from a context menu Fact.
 */
fun Fact.toContextMenuExtraKey() =
    if (component == Component.FEATURE_CONTEXTMENU) {
        metadata?.get("item").toString().removePrefix("mozac.feature.contextmenu.")
    } else {
        throw IllegalArgumentException("Fact is not a context menu fact")
    }
