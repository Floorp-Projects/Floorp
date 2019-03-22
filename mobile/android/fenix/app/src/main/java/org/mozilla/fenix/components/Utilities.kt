/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases

/**
 * Component group for miscellaneous components.
 */
class Utilities(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionUseCases: SessionUseCases,
    private val searchUseCases: SearchUseCases
) {
    /**
     * Provides intent processing functionality for CustomTab, ACTION_VIEW
     * and ACTION_SEND intents.
     */
    val intentProcessor by lazy {
        IntentProcessor(sessionUseCases, sessionManager, searchUseCases, context)
    }

    val privateIntentProcessor by lazy {
        IntentProcessor(sessionUseCases, sessionManager, searchUseCases, context, isPrivate = true)
    }

    val icons by lazy {
        BrowserIcons(context)
    }
}
