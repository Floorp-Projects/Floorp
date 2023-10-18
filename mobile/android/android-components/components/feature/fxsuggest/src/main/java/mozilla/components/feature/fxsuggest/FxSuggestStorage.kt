/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.suggest.SuggestIngestionConstraints
import mozilla.appservices.suggest.SuggestStore
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionQuery
import java.io.File

/**
 * A coroutine-aware wrapper around the synchronous [SuggestStore] interface.
 *
 * @property context The Android application context.
 */
class FxSuggestStorage(
    context: Context,
) {
    // Lazily initializes the store on first use. `cacheDir` and using the `File` constructor
    // does I/O, so `store.value` should only be accessed from the read or write scope.
    private val store: Lazy<SuggestStore> = lazy {
        SuggestStore(File(context.cacheDir, DATABASE_NAME).absolutePath)
    }

    // We expect almost all Suggest storage operations to be reads, with infrequent writes. The
    // I/O dispatcher supports both workloads, and using separate scopes lets us cancel reads
    // without affecting writes.
    private val readScope: CoroutineScope = CoroutineScope(Dispatchers.IO)
    private val writeScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    /**
     * Queries the store for suggestions.
     *
     * @param query The input and suggestion types to match.
     * @return A list of matching suggestions.
     */
    suspend fun query(query: SuggestionQuery): List<Suggestion> =
        withContext(readScope.coroutineContext) {
            store.value.query(query)
        }

    /**
     * Downloads and persists new Firefox Suggest search suggestions.
     *
     * @param constraints Optional limits on suggestions to ingest.
     */
    suspend fun ingest(constraints: SuggestIngestionConstraints = SuggestIngestionConstraints()) =
        withContext(writeScope.coroutineContext) {
            store.value.ingest(constraints)
        }

    /**
     * Interrupts any ongoing queries for suggestions.
     */
    fun cancelReads() {
        if (store.isInitialized()) {
            store.value.interrupt()
            readScope.coroutineContext.cancelChildren()
        }
    }

    internal companion object {
        /**
         * The default Suggest database file name.
         */
        const val DATABASE_NAME = "suggest.sqlite"
    }
}
