/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.intent.TabIntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.test.Mockable

/**
 * Component group for miscellaneous components.
 */
@Mockable
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
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch, isPrivate = false)
    }

    val privateIntentProcessor by lazy {
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch, isPrivate = true)
    }

    val customTabIntentProcessor by lazy {
        CustomTabIntentProcessor(sessionManager, sessionUseCases.loadUrl, context.resources)
    }

    val publicSuffixList by lazy { PublicSuffixList(context) }
}
