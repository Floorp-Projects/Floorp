/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.exceptions

import android.content.Context
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import kotlinx.android.synthetic.main.fragment_exceptions_domains.*
import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.async
import kotlinx.coroutines.experimental.launch
import org.mozilla.focus.R
import org.mozilla.focus.settings.BaseSettingsFragment

class ExceptionsRemoveFragment : ExceptionsListFragment() {
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
        val domains = (exceptionList.adapter as DomainListAdapter).selection()
        if (domains.isNotEmpty()) {
            launch(UI) {
                async {
                    ExceptionDomains.remove(context, domains)
                }.await()

                fragmentManager!!.popBackStack()
            }
        }
    }

    override fun isSelectionMode() = true

    override fun onResume() {
        super.onResume()

        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_title_remove)
        updater.updateIcon(R.drawable.ic_back)
    }
}
