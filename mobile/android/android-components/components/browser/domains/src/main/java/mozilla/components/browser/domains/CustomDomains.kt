/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.content.Context
import android.content.SharedPreferences

/**
 * Contains functionality to manage custom domains for auto-completion.
 */
object CustomDomains {
    private const val PREFERENCE_NAME = "custom_autocomplete"
    private const val KEY_DOMAINS = "custom_domains"
    private const val SEPARATOR = "@<;>@"

    /**
     * Loads the previously added/saved custom domains from preferences.
     *
     * @param context the application context
     * @return list of custom domains
     */
    fun load(context: Context): List<String> =
        preferences(context).getString(KEY_DOMAINS, "")!!
            .split(SEPARATOR)
            .filter { !it.isEmpty() }

    /**
     * Saves the provided domains to preferences.
     *
     * @param context the application context
     * @param domains list of domains
     */
    fun save(context: Context, domains: List<String>) {
        preferences(context)
            .edit()
            .putString(KEY_DOMAINS, domains.joinToString(separator = SEPARATOR))
            .apply()
    }

    /**
     * Adds the provided domain to preferences.
     *
     * @param context the application context
     * @param domain the domain to add
     */
    fun add(context: Context, domain: String) {
        val domains = mutableListOf<String>()
        domains.addAll(load(context))
        domains.add(domain)

        save(context, domains)
    }

    /**
     * Removes the provided domain from preferences.
     *
     * @param context the application context
     * @param domains the domain to remove
     */
    fun remove(context: Context, domains: List<String>) {
        save(context, load(context) - domains)
    }

    private fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
}
