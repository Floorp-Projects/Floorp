/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.cookiebanners

import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.emptyPreferences
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import mozilla.components.browser.engine.gecko.cookiebanners.ReportSiteDomainsRepository.PreferencesKeys.REPORT_SITE_DOMAINS
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException

/**
 * A repository to save reported site domains with the datastore API.
 */
class ReportSiteDomainsRepository(
    private val dataStore: DataStore<Preferences>,
) {

    companion object {
        const val SEPARATOR = "@<;>@"
        const val REPORT_SITE_DOMAINS_REPOSITORY_NAME = "report_site_domains_preferences"
        const val PREFERENCE_KEY_NAME = "report_site_domains"
    }

    private object PreferencesKeys {
        val REPORT_SITE_DOMAINS = stringPreferencesKey(PREFERENCE_KEY_NAME)
    }

    /**
     * Check if the given site's domain url is saved locally.
     * @param siteDomain the [siteDomain] that will be checked.
     */
    suspend fun isSiteDomainReported(siteDomain: String): Boolean {
        return dataStore.data
            .catch { exception ->
                if (exception is IOException) {
                    Logger.error("Error reading preferences.", exception)
                    emit(emptyPreferences())
                } else {
                    throw exception
                }
            }.map { preferences ->
                val reportSiteDomainsString = preferences[REPORT_SITE_DOMAINS] ?: ""
                val reportSiteDomainsList =
                    reportSiteDomainsString.split(SEPARATOR).filter { it.isNotEmpty() }
                reportSiteDomainsList.contains(siteDomain)
            }.first()
    }

    /**
     * Save the given site's domain url in datastore to keep it persistent locally.
     * This method gets called after the site domain was reported with Nimbus.
     * @param siteDomain the [siteDomain] that will be saved.
     */
    suspend fun saveSiteDomain(siteDomain: String) {
        dataStore.edit { preferences ->
            val siteDomainsPreferences = preferences[REPORT_SITE_DOMAINS] ?: ""
            val siteDomainsList = siteDomainsPreferences.split(SEPARATOR).filter { it.isNotEmpty() }
            if (siteDomainsList.contains(siteDomain)) {
                return@edit
            }
            val domains = mutableListOf<String>()
            domains.addAll(siteDomainsList)
            domains.add(siteDomain)
            preferences[REPORT_SITE_DOMAINS] = domains.joinToString(SEPARATOR)
        }
    }
}
