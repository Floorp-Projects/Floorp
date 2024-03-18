/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

/**
 * Provides global access to the dependencies needed to access Firefox Suggest search suggestions.
 */
object GlobalFxSuggestDependencyProvider {
    internal var storage: FxSuggestStorage? = null

    /**
     * Initializes this provider with a wrapped Suggest store.
     *
     * Your application's [onCreate][android.app.Application.onCreate] method should call this
     * method once.
     *
     * @param storage The wrapped Suggest store.
     */
    fun initialize(storage: FxSuggestStorage) {
        this.storage = storage
    }

    internal fun requireStorage(): FxSuggestStorage {
        return requireNotNull(storage) {
            "`GlobalFxSuggestDependencyProvider.initialize` must be called before accessing `storage`"
        }
    }
}
