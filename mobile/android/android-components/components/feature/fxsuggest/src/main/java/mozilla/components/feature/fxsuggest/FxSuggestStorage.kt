/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.suggest.SuggestApiException
import mozilla.appservices.suggest.SuggestIngestionConstraints
import mozilla.appservices.suggest.SuggestStore
import mozilla.appservices.suggest.SuggestStoreBuilder
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionQuery
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger
import java.io.File

/**
 * A coroutine-aware wrapper around the synchronous [SuggestStore] interface.
 *
 * @param context The Android application context.
 * @param crashReporter An optional [CrashReporting] instance for reporting unexpected caught
 * exceptions.
 */
class FxSuggestStorage(context: Context) {
    // Lazily initializes the store on first use. `cacheDir` and using the `File` constructor
    // does I/O, so `store.value` should only be accessed from the read or write scope.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val store: Lazy<SuggestStore> = lazy {
        SuggestStoreBuilder()
            .cachePath(File(context.cacheDir, CACHE_DATABASE_NAME).absolutePath)
            .dataPath(context.getDatabasePath(DATABASE_NAME).absolutePath)
            .build()
    }

    // We expect almost all Suggest storage operations to be reads, with infrequent writes. The
    // I/O dispatcher supports both workloads, and using separate scopes lets us cancel reads
    // without affecting writes.
    private val readScope: CoroutineScope = CoroutineScope(Dispatchers.IO)
    private val writeScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    private val logger = Logger("FxSuggestStorage")

    /**
     * Queries the store for suggestions.
     *
     * @param query The input and suggestion types to match.
     * @return A list of matching suggestions.
     */
    suspend fun query(query: SuggestionQuery): List<Suggestion> =
        withContext(readScope.coroutineContext) {
            handleSuggestExceptions("query", emptyList()) {
                store.value.query(query)
            }
        }

    /**
     * Downloads and persists new Firefox Suggest search suggestions.
     *
     * @param constraints Optional limits on suggestions to ingest.
     * @return `true` if ingestion succeeded; `false` if ingestion failed and should be retried.
     */
    suspend fun ingest(constraints: SuggestIngestionConstraints = SuggestIngestionConstraints()): Boolean =
        withContext(writeScope.coroutineContext) {
            handleSuggestExceptions("ingest", false) {
                store.value.ingest(constraints)
                true
            }
        }

    /**
     * Run startup ingestion
     *
     * This will run ingestion, only if there are currently no suggestions in the database.  This is
     * used to initialize the database on first startup and also after Firefox updates that change
     * the schema (which often cause the suggestions table to be cleared).
     */
    suspend fun runStartupIngestion() {
        logger.info("runStartupIngestion")
        ingest(SuggestIngestionConstraints(emptyOnly = true))
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

    /**
     * Runs an [operation] with the given [name], ignoring and logging any non-fatal exceptions.
     * Returns either the result of the [operation], or the provided [default] value if the
     * [operation] throws an exception.
     *
     * @param name The name of the operation to run.
     * @param default The default value to return if the operation fails.
     * @param operation The operation to run.
     */
    private inline fun <T> handleSuggestExceptions(
        name: String,
        default: T,
        operation: () -> T,
    ): T {
        return try {
            operation()
        } catch (e: SuggestApiException) {
            logger.warn("Ignoring exception from `$name`", e)
            default
        }
    }

    internal companion object {
        /**
         * The database file name for cached data.
         */
        const val CACHE_DATABASE_NAME = "suggest.sqlite"

        /**
         * The database file name for permanent data.
         */
        const val DATABASE_NAME = "suggest_data.sqlite"
    }
}
