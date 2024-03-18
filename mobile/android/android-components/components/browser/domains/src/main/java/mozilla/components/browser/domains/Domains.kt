/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.content.Context
import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES
import android.os.LocaleList
import android.text.TextUtils
import java.io.IOException
import java.util.Locale

/**
 * Contains functionality to access domain lists shipped as part of this
 * module's assets.
 */
object Domains {

    /**
     * Loads the domains applicable to the app's locale, plus the domains
     * in the 'global' list.
     *
     * @param context the application context
     * @return list of domains
     */
    fun load(context: Context): List<String> {
        return load(context, getCountriesInDefaultLocaleList())
    }

    internal fun load(context: Context, countries: Set<String>): List<String> {
        val domains = LinkedHashSet<String>()
        val availableLists = getAvailableDomainLists(context)

        // First initialize the country specific lists following the default locale order
        countries
            .filter { availableLists.contains(it) }
            .forEach { loadDomainsForLanguage(context, domains, it) }

        // And then add domains from the global list
        loadDomainsForLanguage(context, domains, "global")

        return domains.toList()
    }

    private fun getAvailableDomainLists(context: Context): Set<String> {
        val availableDomains = LinkedHashSet<String>()
        val assetManager = context.assets
        val domains = try {
            assetManager.list("domains") ?: emptyArray<String>()
        } catch (e: IOException) {
            emptyArray<String>()
        }
        availableDomains.addAll(domains)
        return availableDomains
    }

    private fun loadDomainsForLanguage(context: Context, domains: MutableSet<String>, country: String) {
        val assetManager = context.assets
        val languageDomains = try {
            assetManager.open("domains/$country").bufferedReader().readLines()
        } catch (e: IOException) {
            emptyList<String>()
        }
        domains.addAll(languageDomains)
    }

    private fun getCountriesInDefaultLocaleList(): Set<String> {
        val countries = java.util.LinkedHashSet<String>()
        val addIfNotEmpty = { c: String -> if (!TextUtils.isEmpty(c)) countries.add(c.lowercase(Locale.US)) }

        if (SDK_INT >= VERSION_CODES.N) {
            val list = LocaleList.getDefault()
            for (i in 0 until list.size()) {
                addIfNotEmpty(list.get(i).country)
            }
        } else {
            addIfNotEmpty(Locale.getDefault().country)
        }

        return countries
    }
}
