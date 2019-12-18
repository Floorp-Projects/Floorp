/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.browser.search.SearchEngine
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.SearchAccessPoint
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.ext.searchEngineManager

object MetricsUtils {
    fun createSearchEvent(
        engine: SearchEngine,
        context: Context,
        searchAccessPoint: SearchAccessPoint
    ): Event.PerformedSearch? {
        val isShortcut = engine != context.searchEngineManager.defaultSearchEngine
        val isCustom = CustomSearchEngineStore.isCustomSearchEngine(context, engine.identifier)

        val engineSource =
            if (isShortcut) Event.PerformedSearch.EngineSource.Shortcut(engine, isCustom)
            else Event.PerformedSearch.EngineSource.Default(engine, isCustom)

        return when (searchAccessPoint) {
            SearchAccessPoint.SUGGESTION -> Event.PerformedSearch(
                Event.PerformedSearch.EventSource.Suggestion(
                    engineSource
                )
            )
            SearchAccessPoint.ACTION -> Event.PerformedSearch(
                Event.PerformedSearch.EventSource.Action(
                    engineSource
                )
            )
            SearchAccessPoint.WIDGET -> Event.PerformedSearch(
                Event.PerformedSearch.EventSource.Widget(
                    engineSource
                )
            )
            SearchAccessPoint.SHORTCUT -> Event.PerformedSearch(
                Event.PerformedSearch.EventSource.Shortcut(
                    engineSource
                )
            )
            SearchAccessPoint.NONE -> Event.PerformedSearch(
                Event.PerformedSearch.EventSource.Other(
                    engineSource
                )
            )
        }
    }
}
