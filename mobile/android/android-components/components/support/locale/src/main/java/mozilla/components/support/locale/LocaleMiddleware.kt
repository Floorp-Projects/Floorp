/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger
import java.util.Locale
import kotlin.coroutines.CoroutineContext

/**
 * [Middleware] implementation for updating [BrowserState.locale] state changes during restore.
 */
class LocaleMiddleware(
    private val applicationContext: Context,
    coroutineContext: CoroutineContext = Dispatchers.IO,
    private val localeManager: LocaleManager,
) : Middleware<BrowserState, BrowserAction> {

    private val logger = Logger("LocaleMiddleware")
    private var scope = CoroutineScope(coroutineContext)

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is LocaleAction.RestoreLocaleStateAction -> restoreLocale(context.store)
            is LocaleAction.UpdateLocaleAction -> updateLocale(action.locale)
            else -> {
                // no-op
            }
        }

        next(action)
    }

    private fun restoreLocale(store: Store<BrowserState, BrowserAction>) = scope.launch {
        val localeHistory = localeManager.getCurrentLocale(applicationContext)
        if (localeHistory == null) {
            logger.debug(
                "No recoverable locale has been set. Following device locale.",
            )
        } else {
            logger.debug("Locale restored from the storage $localeHistory")
        }
        store.dispatch(LocaleAction.UpdateLocaleAction(localeHistory))
    }

    private fun updateLocale(locale: Locale?) {
        localeManager.setNewLocale(applicationContext, locale = locale)
    }
}
