/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.support.ktx.android.content.PreferencesHolder
import mozilla.components.support.ktx.android.content.stringPreference

/**
 * Contains functionality to manage custom domains for allow-list.
 */
class ExceptionDomains(context: Context) : PreferencesHolder {

    override val preferences: SharedPreferences =
        context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)

    private var domains by stringPreference(KEY_DOMAINS, default = "")

    /**
     * Loads the previously added/saved custom domains from preferences.
     *
     * @return list of custom domains
     */
    fun load(): List<String> {
        if (exceptions == null) {
            exceptions = domains.split(SEPARATOR).filter { it.isNotEmpty() }
        }

        return exceptions.orEmpty()
    }

    /**
     * Saves the provided domains to preferences.
     *
     * @param domains list of domains
     */
    fun save(domains: List<String>) {
        exceptions = domains

        this.domains = domains.joinToString(separator = SEPARATOR)
    }

    /**
     * Adds the provided domain to preferences.
     *
     * @param domain the domain to add
     */
    fun add(domain: String) {
        save(domains = load() + domain)
    }

    /**
     * Removes the provided domain from preferences.
     *
     * @param domains the domain to remove
     */
    fun remove(domains: List<String>) {
        save(domains = load() - domains)
    }

    /**
     * Adds or removes the provided domain from preferences.
     *
     * If present, the domain will be removed. Otherwise, it will be added.
     */
    fun toggle(domain: String) {
        if (domain in load()) {
            remove(listOf(domain))
        } else {
            add(domain)
        }
    }

    companion object {
        private const val PREFERENCE_NAME = "exceptions"
        private const val KEY_DOMAINS = "exceptions_domains"
        private const val SEPARATOR = "@<;>@"

        private var exceptions: List<String>? = null
    }
}
