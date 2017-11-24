/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.content.SharedPreferences
import edu.umd.cs.findbugs.annotations.SuppressFBWarnings

@SuppressFBWarnings("ST_WRITE_TO_STATIC_FROM_INSTANCE_METHOD") // That's just how Kotlin singletons work...
object CustomAutocomplete {
    private const val PREFERENCE_NAME = "custom_autocomplete"
    private const val KEY_DOMAINS = "domains"

    suspend fun loadCustomAutoCompleteDomains(context: Context): Set<String> =
            preferences(context).getStringSet(KEY_DOMAINS, HashSet())

    fun saveDomains(context: Context, domains: Set<String>) {
        preferences(context)
                .edit()
                .putStringSet(KEY_DOMAINS, domains)
                .apply()
    }

    suspend fun addDomain(context: Context, domain: String) {
        val domains = mutableSetOf<String>()
        domains.addAll(loadCustomAutoCompleteDomains(context))
        domains.add(domain)

        saveDomains(context, domains)
    }

    suspend fun removeDomains(context: Context, domains: Set<String>) {
        saveDomains(context, loadCustomAutoCompleteDomains(context) - domains)
    }

    fun subscribe(context: Context, listener: SharedPreferences.OnSharedPreferenceChangeListener) {
        preferences(context).registerOnSharedPreferenceChangeListener(listener)
    }

    fun unsubscribe(context: Context, listener: SharedPreferences.OnSharedPreferenceChangeListener) {
        preferences(context).unregisterOnSharedPreferenceChangeListener(listener)
    }

    private fun preferences(context: Context): SharedPreferences =
            context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
}
