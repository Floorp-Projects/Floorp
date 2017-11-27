/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.app.Fragment
import android.content.Context
import android.os.Bundle
import android.view.*
import org.mozilla.focus.R
import org.mozilla.focus.settings.SettingsFragment

import kotlinx.android.synthetic.main.fragment_autocomplete_add_domain.*
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.launch
import org.mozilla.focus.ext.removePrefixesIgnoreCase
import org.mozilla.focus.utils.ViewUtils

/**
 * Fragment showing settings UI to add custom autocomplete domains.
 */
class AutocompleteAddFragment : Fragment() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()

        val updater = activity as SettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_title_add)
        updater.updateIcon(R.drawable.ic_close)
    }

    override fun onCreateView(inflater: LayoutInflater?, container: ViewGroup?, savedInstanceState: Bundle?): View =
            inflater!!.inflate(R.layout.fragment_autocomplete_add_domain, container, false)

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        ViewUtils.showKeyboard(domainView)
    }

    override fun onCreateOptionsMenu(menu: Menu?, inflater: MenuInflater?) {
        inflater?.inflate(R.menu.menu_autocomplete_add, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem?): Boolean {
        if (item?.itemId == R.id.save) {

            val domain = domainView.text.toString()
                    .trim()
                    .toLowerCase()
                    .removePrefixesIgnoreCase("http://", "https://", "www.")

            if (domain.isEmpty()) {
                domainView.error = getString(R.string.preference_autocomplete_add_error)
            } else {
                saveDomainAndClose(activity.applicationContext, domain)
            }

            return true
        }

        return super.onOptionsItemSelected(item)
    }

    private fun saveDomainAndClose(context: Context, domain: String) {
        launch(CommonPool) {
            CustomAutocomplete.addDomain(context, domain)
        }

        ViewUtils.showBrandedSnackbar(view, R.string.preference_autocomplete_add_confirmation, 0)

        fragmentManager.popBackStack()
    }
}

