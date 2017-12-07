/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.content.SharedPreferences

object CustomAutocomplete {
    private const val PREFERENCE_NAME = "custom_autocomplete"
    private const val KEY_DOMAINS = "custom_domains"
    private const val SEPARATOR = "@<;>@"

    suspend fun loadCustomAutoCompleteDomains(context: Context): List<String> =
            preferences(context).getString(KEY_DOMAINS, "")
                .split(SEPARATOR)
                .filter { !it.isEmpty() }

    fun saveDomains(context: Context, domains: List<String>) {
        preferences(context)
                .edit()
                .putString(KEY_DOMAINS, domains.joinToString(separator = SEPARATOR))
                .apply()
    }

    suspend fun addDomain(context: Context, domain: String) {
        val domains = mutableListOf<String>()
        domains.addAll(loadCustomAutoCompleteDomains(context))
        domains.add(domain)

        saveDomains(context, domains)
    }

    suspend fun removeDomains(context: Context, domains: List<String>) {
        saveDomains(context, loadCustomAutoCompleteDomains(context) - domains)
    }

    private fun preferences(context: Context): SharedPreferences =
            context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
}
