/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.database.sqlite.SQLiteBlobTooBigException
import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.db.CrashDatabase

/**
 * Fragment displaying the list of crashes.
 */
internal class CrashListFragment : Fragment(R.layout.mozac_lib_crash_crashlist) {
    private val database by lazy { CrashDatabase.get(requireContext()) }
    private val reporter by lazy { (activity as AbstractCrashListActivity).crashReporter }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val listView: RecyclerView = view.findViewById(R.id.mozac_lib_crash_list)
        listView.layoutManager = LinearLayoutManager(
            requireContext(),
            LinearLayoutManager.VERTICAL,
            false,
        )

        val emptyView = view.findViewById<TextView>(R.id.mozac_lib_crash_empty)

        val adapter = CrashListAdapter(reporter, ::onSelection)
        listView.adapter = adapter

        val dividerItemDecoration = DividerItemDecoration(
            requireContext(),
            LinearLayoutManager.VERTICAL,
        )
        listView.addItemDecoration(dividerItemDecoration)

        try {
            database.crashDao().getCrashesWithReports().observe(
                viewLifecycleOwner,
                Observer { list ->
                    if (list.isEmpty()) {
                        emptyView.visibility = View.VISIBLE
                    } else {
                        adapter.updateList(list)
                    }
                },
            )
        } catch (e: SQLiteBlobTooBigException) {
            /* recover by deleting all entries */
            database.crashDao().deleteAll()
        }
    }

    private fun onSelection(url: String) {
        (requireActivity() as AbstractCrashListActivity).onCrashServiceSelected(url)
    }
}
