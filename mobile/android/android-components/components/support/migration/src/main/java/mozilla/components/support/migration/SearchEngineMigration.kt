/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.support.base.log.logger.Logger

private const val PREF_FENNEC_DEFAULT_ENGINE_KEY = "search.engines.defaultname"
private const val PREF_FENIX_DEFAULT_ENGINE_KEY = "pref_key_search_engine"

/**
 * Implementation for migrating the default search engine from Fennec to Fenix.
 *
 * This is a "best effort" migration. We will recover the default search engine name from Fennec and
 * then try to find a matching search engine in the search engine manager for the current Fenix
 * installation. If we find a match we will update the default search engine to that; otherwise
 * the regular default search engine for this installation will be used.
 */
internal object SearchEngineMigration {
    private val logger = Logger("SearchEngineMigration")

    /**
     * Tries to migrate the default search engine from Fennec to Fenix.
     */
    suspend fun migrate(
        context: Context,
        searchEngineManager: SearchEngineManager
    ): Result<SearchEngineMigrationResult> {
        val fennecSearchEngine = retrieveFennecDefaultSearchEngineName(context)
        if (fennecSearchEngine == null) {
            logger.debug("Could not locate Fennec search engine preference")
            return Result.Failure(
                SearchEngineMigrationException(SearchEngineMigrationResult.Failure.NoDefault))
        }

        logger.debug("Found search engine from Fennec: $fennecSearchEngine")

        return determineNewDefault(context, searchEngineManager, fennecSearchEngine)
    }

    private suspend fun determineNewDefault(
        context: Context,
        searchEngineManager: SearchEngineManager,
        name: String
    ): Result<SearchEngineMigrationResult> {
        val searchEngines = searchEngineManager.getSearchEnginesAsync(context)

        logger.debug("Got ${searchEngines.size} search engines from SearchEngineManager.")

        // We will try to find a search engine that has a matching name in the list of search engines
        // for this Fenix installation (which depends on locale/region). If we find a match then we
        // update Fenix to use this search engine. If there's no match then we leave it to Fenix to
        // pick a default search engine.
        searchEngines.forEach { engine ->
            logger.debug(" - Fennec: $name - Comparing with Fenix search engine: ${engine.name}")

            if (engine.name.contains(name, ignoreCase = true)) {
                logger.debug("Setting new default: ${engine.name}")

                val fenixPreferences = context.getSharedPreferences(
                    FennecSettingsMigration.FENIX_SHARED_PREFS_NAME,
                    Context.MODE_PRIVATE
                )

                fenixPreferences.edit()
                    .putString(PREF_FENIX_DEFAULT_ENGINE_KEY, engine.name)
                    .apply()

                searchEngineManager.defaultSearchEngine = engine

                return Result.Success(SearchEngineMigrationResult.Success.SearchEngineMigrated)
            }
        }

        logger.debug("Could not find matching search engine")
        return Result.Failure(SearchEngineMigrationException(
            SearchEngineMigrationResult.Failure.NoMatch
        ))
    }

    private fun retrieveFennecDefaultSearchEngineName(context: Context): String? {
        val fennecPreferences = context.getSharedPreferences(
            FennecSettingsMigration.FENNEC_APP_SHARED_PREFS_NAME,
            Context.MODE_PRIVATE
        )

        return fennecPreferences.getString(PREF_FENNEC_DEFAULT_ENGINE_KEY, null)
    }
}

/**
 * Result of the default search engine migration.
 */
internal sealed class SearchEngineMigrationResult {
    internal sealed class Success : SearchEngineMigrationResult() {
        internal object SearchEngineMigrated : Success()
    }

    internal sealed class Failure : SearchEngineMigrationResult() {
        internal object NoDefault : SearchEngineMigrationResult.Failure()
        internal object NoMatch : SearchEngineMigrationResult.Failure()
    }
}

/**
 * Wraps [SearchEngineMigrationResult] in an exception so that it can be returned via [Result.Failure].
 */
internal class SearchEngineMigrationException(
    val failure: SearchEngineMigrationResult.Failure
) : Exception(failure.toString())
