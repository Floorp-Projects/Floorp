/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.intent.TrustedWebActivityIntentProcessor
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.support.migration.MigrationIntentProcessor
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.customtabs.FennecWebAppIntentProcessor
import org.mozilla.fenix.home.intent.FennecBookmarkShortcutsIntentProcessor
import org.mozilla.fenix.utils.Mockable

/**
 * Component group for miscellaneous components.
 */
@Mockable
class IntentProcessors(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionUseCases: SessionUseCases,
    private val searchUseCases: SearchUseCases,
    private val relationChecker: RelationChecker,
    private val customTabsStore: CustomTabsServiceStore,
    private val migrationStore: MigrationStore,
    private val manifestStorage: ManifestStorage
) {
    /**
     * Provides intent processing functionality for ACTION_VIEW and ACTION_SEND intents.
     */
    val intentProcessor by lazy {
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch, isPrivate = false)
    }

    /**
     * Provides intent processing functionality for ACTION_VIEW and ACTION_SEND intents in private tabs.
     */
    val privateIntentProcessor by lazy {
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch, isPrivate = true)
    }

    val customTabIntentProcessor by lazy {
        CustomTabIntentProcessor(sessionManager, sessionUseCases.loadUrl, context.resources, isPrivate = false)
    }

    val privateCustomTabIntentProcessor by lazy {
        CustomTabIntentProcessor(sessionManager, sessionUseCases.loadUrl, context.resources, isPrivate = true)
    }

    val externalAppIntentProcessors by lazy {
        listOf(
            TrustedWebActivityIntentProcessor(
                sessionManager = sessionManager,
                loadUrlUseCase = sessionUseCases.loadUrl,
                packageManager = context.packageManager,
                relationChecker = relationChecker,
                store = customTabsStore
            ),
            WebAppIntentProcessor(sessionManager, sessionUseCases.loadUrl, manifestStorage),
            FennecWebAppIntentProcessor(context, sessionManager, sessionUseCases.loadUrl, manifestStorage)
        )
    }

    val fennecPageShortcutIntentProcessor by lazy {
        FennecBookmarkShortcutsIntentProcessor(sessionManager, sessionUseCases.loadUrl)
    }

    val migrationIntentProcessor by lazy {
        MigrationIntentProcessor(migrationStore)
    }
}
