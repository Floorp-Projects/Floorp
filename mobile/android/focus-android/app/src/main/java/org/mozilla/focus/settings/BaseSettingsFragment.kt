/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.Context
import android.os.Bundle
import android.support.v4.app.Fragment
import android.support.v7.preference.PreferenceFragmentCompat
import android.view.View
import android.widget.ListView
import org.mozilla.focus.R

abstract class BaseSettingsFragment : PreferenceFragmentCompat() {

    interface ActionBarUpdater {
        fun updateTitle(titleResId: Int)
        fun updateIcon(iconResId: Int)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val list = view.findViewById<View>(android.R.id.list) as? ListView
        list?.divider = null
    }

    override fun onAttach(context: Context?) {
        super.onAttach(context)
        if (activity !is ActionBarUpdater) {
            throw IllegalArgumentException("Parent activity must implement ActionBarUpdater")
        }
    }

    protected fun getActionBarUpdater() = activity as ActionBarUpdater

    protected fun navigateToFragment(fragment: Fragment) {
        fragmentManager!!.beginTransaction()
            .replace(R.id.container, fragment)
            .addToBackStack(null)
            .commit()
    }
}
