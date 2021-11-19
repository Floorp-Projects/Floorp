/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.intent.TrustedWebActivityIntentProcessor
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.digitalassetlinks.RelationChecker
import org.mozilla.fenix.customtabs.FennecWebAppIntentProcessor
import org.mozilla.fenix.home.intent.FennecBookmarkShortcutsIntentProcessor
import org.mozilla.fenix.intent.ExternalDeepLinkIntentProcessor
import org.mozilla.fenix.perf.lazyMonitored

/**
 * Component group for miscellaneous components.
 */
@Suppress("LongParameterList")
class IntentProcessors(
    private val context: Context,
    private val store: BrowserStore,
    private val sessionUseCases: SessionUseCases,
    private val tabsUseCases: TabsUseCases,
    private val customTabsUseCases: CustomTabsUseCases,
    private val searchUseCases: SearchUseCases,
    private val relationChecker: RelationChecker,
    private val customTabsStore: CustomTabsServiceStore,
    private val manifestStorage: ManifestStorage
) {
    /**
     * Provides intent processing functionality for ACTION_VIEW and ACTION_SEND intents.
     */
    val intentProcessor by lazyMonitored {
        TabIntentProcessor(tabsUseCases, searchUseCases.newTabSearch, isPrivate = false)
    }

    /**
     * Provides intent processing functionality for ACTION_VIEW and ACTION_SEND intents in private tabs.
     */
    val privateIntentProcessor by lazyMonitored {
        TabIntentProcessor(tabsUseCases, searchUseCases.newTabSearch, isPrivate = true)
    }

    val customTabIntentProcessor by lazyMonitored {
        CustomTabIntentProcessor(customTabsUseCases.add, context.resources, isPrivate = false)
    }

    val privateCustomTabIntentProcessor by lazyMonitored {
        CustomTabIntentProcessor(customTabsUseCases.add, context.resources, isPrivate = true)
    }

    val externalDeepLinkIntentProcessor by lazyMonitored {
        ExternalDeepLinkIntentProcessor()
    }

    val externalAppIntentProcessors by lazyMonitored {
        listOf(
            TrustedWebActivityIntentProcessor(
                addNewTabUseCase = customTabsUseCases.add,
                packageManager = context.packageManager,
                relationChecker = relationChecker,
                store = customTabsStore
            ),
            WebAppIntentProcessor(store, customTabsUseCases.addWebApp, sessionUseCases.loadUrl, manifestStorage),
            FennecWebAppIntentProcessor(context, customTabsUseCases, manifestStorage)
        )
    }

    val fennecPageShortcutIntentProcessor by lazyMonitored {
        FennecBookmarkShortcutsIntentProcessor(tabsUseCases.addTab)
    }
}
