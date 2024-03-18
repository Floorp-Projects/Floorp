/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.action.LocaleAction.UpdateLocaleAction
import mozilla.components.browser.state.store.BrowserStore
import java.util.Locale

/**
 * Contains use cases related to localization.
 */
class LocaleUseCases(browserStore: BrowserStore) {
    /**
     * Updates the [Locale] to the most recent user selection.
     */
    class UpdateLocaleUseCase internal constructor(
        private val store: BrowserStore,
    ) {
        /**
         * Updates the locale for [BrowserStore] observers.
         *
         * @param locale The [Locale] that has been selected.
         */
        operator fun invoke(locale: Locale?) {
            store.dispatch(UpdateLocaleAction(locale))
        }
    }

    /**
     * Use case for restoring the [Locale].
     */
    class RestoreUseCase(
        private val browserStore: BrowserStore,
    ) {
        /**
         * Restores the given [Locale] from storage.
         */
        operator fun invoke() {
            browserStore.dispatch(LocaleAction.RestoreLocaleStateAction)
        }
    }

    val notifyLocaleChanged: UpdateLocaleUseCase by lazy {
        UpdateLocaleUseCase(browserStore)
    }

    val restore: RestoreUseCase by lazy {
        RestoreUseCase(browserStore)
    }
}
