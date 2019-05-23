/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import org.mozilla.fenix.R


class DeleteBrowsingDataFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_delete_browsing_data, container, false)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).apply {
            title = getString(R.string.preferences_delete_browsing_data)
            supportActionBar?.show()
        }
    }
}
