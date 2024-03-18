/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.dataprotect

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.dataprotect.SecureAbove22Preferences

class MainActivity : AppCompatActivity() {
    @Suppress("MagicNumber")
    private val itemKeys: List<String> = List(5) { "protected item ${it + 1}" }

    private lateinit var listView: RecyclerView
    private lateinit var listAdapter: ProtectedDataAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val prefs = SecureAbove22Preferences(this, "secret-data-storage")

        prepareProtectedData(prefs)

        // setup recycler
        listAdapter = ProtectedDataAdapter(prefs, itemKeys)
        listView = findViewById(R.id.protecteddata_list)
        listView.apply {
            setHasFixedSize(true)
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = listAdapter
        }
    }

    private fun prepareProtectedData(prefs: SecureAbove22Preferences) {
        for (datakey in itemKeys) {
            val plain = "value for $datakey"
            prefs.putString(datakey, plain)
        }
    }
}
