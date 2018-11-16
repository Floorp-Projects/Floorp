/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import kotlinx.android.synthetic.main.fragment_autocomplete_customdomains.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.CustomDomains
import org.mozilla.focus.R
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.telemetry.TelemetryWrapper
import kotlin.coroutines.CoroutineContext

class AutocompleteRemoveFragment : AutocompleteListFragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    override fun onCreateOptionsMenu(menu: Menu?, inflater: MenuInflater?) {
        inflater?.inflate(R.menu.menu_autocomplete_remove, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem?): Boolean = when (item?.itemId) {
        R.id.remove -> {
            removeSelectedDomains(activity!!.applicationContext)
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    private fun removeSelectedDomains(context: Context) {
        val domains = (domainList.adapter as DomainListAdapter).selection()
        if (domains.isNotEmpty()) {
            launch(Main) {
                async {
                    CustomDomains.remove(context, domains)

                    TelemetryWrapper.removeAutocompleteDomainsEvent(domains.size)
                }.await()

                fragmentManager!!.popBackStack()
            }
        }
    }

    override fun isSelectionMode() = true

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_title_remove)
        updater.updateIcon(R.drawable.ic_back)
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
    }
}
