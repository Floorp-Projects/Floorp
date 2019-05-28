/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.os.Bundle
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_autocomplete_add_domain.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.CustomDomains
import org.mozilla.focus.R
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.ViewUtils
import kotlin.coroutines.CoroutineContext

/**
 * Fragment showing settings UI to add custom autocomplete domains.
 */
class AutocompleteAddFragment : Fragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_title_add)
        updater.updateIcon(R.drawable.ic_close)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
            inflater.inflate(R.layout.fragment_autocomplete_add_domain, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        ViewUtils.showKeyboard(domainView)
    }

    override fun onPause() {
        job.cancel()
        ViewUtils.hideKeyboard(activity?.currentFocus)
        super.onPause()
    }

    override fun onCreateOptionsMenu(menu: Menu?, inflater: MenuInflater?) {
        inflater?.inflate(R.menu.menu_autocomplete_add, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem?): Boolean {
        if (item?.itemId == R.id.save) {

            val domain = domainView.text.toString()
                    .trim()
                    .toLowerCase()

            launch(IO) {
                val domains = CustomDomains.load(activity!!)
                val error = when {
                    domain.isEmpty() -> getString(R.string.preference_autocomplete_add_error)
                    domains.contains(domain) -> getString(R.string.preference_autocomplete_duplicate_url_error)
                    else -> null
                }

                launch(Main) {
                    if (error != null) {
                        domainView.error = error
                    } else {
                        saveDomainAndClose(activity!!.applicationContext, domain)
                    }
                }
            }

            return true
        }

        return super.onOptionsItemSelected(item)
    }

    private fun saveDomainAndClose(context: Context, domain: String) {
        launch(IO) {
            CustomDomains.add(context, domain)

            TelemetryWrapper.saveAutocompleteDomainEvent(TelemetryWrapper.AutoCompleteEventSource.SETTINGS)
        }

        ViewUtils.showBrandedSnackbar(view, R.string.preference_autocomplete_add_confirmation, 0)

        fragmentManager?.popBackStack()
    }
}
