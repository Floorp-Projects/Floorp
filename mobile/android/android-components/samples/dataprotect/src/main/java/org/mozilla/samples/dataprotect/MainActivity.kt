/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.dataprotect

import android.content.SharedPreferences
import android.os.Bundle
import android.util.Base64
import android.widget.Button
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.dataprotect.Keystore
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.samples.dataprotect.Constants.B64_FLAGS
import org.mozilla.samples.dataprotect.Constants.KEYSTORE_LABEL
import java.nio.charset.StandardCharsets

class MainActivity : AppCompatActivity() {
    private val logger: Logger = Logger("dataprotect")
    private val keystore: Keystore = Keystore(KEYSTORE_LABEL)
    @Suppress("MagicNumber")
    private val itemKeys: List<String> = List(5) { "protected item ${it + 1}" }

    private lateinit var listView: RecyclerView
    private lateinit var listAdapter: ProtectedDataAdapter
    private lateinit var toggleBtn: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // initialize keystore label
        keystore.generateKey()

        // setup protected data key/value pairs
        val prefs = getSharedPreferences(SAMPLE_PREFS_KEY, MODE_PRIVATE).apply {
            prepareProtectedData(this)
        }

        // setup recycler
        listAdapter = ProtectedDataAdapter(prefs, keystore, itemKeys)
        listView = findViewById(R.id.protecteddata_list)
        listView.apply {
            setHasFixedSize(true)
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = listAdapter
        }

        toggleBtn = findViewById(R.id.toggle_locked_btn)
        toggleBtn.setOnClickListener { onToggleLockedClicked() }
        updateToggleButton()
    }

    private fun prepareProtectedData(prefs: SharedPreferences) {
        val editor = prefs.edit()
        for (datakey in itemKeys) {
            if (!prefs.contains(datakey)) {
                val plain = "value for $datakey"
                val encrypted = keystore.encryptBytes(plain.toByteArray(StandardCharsets.UTF_8))
                val dataval = Base64.encodeToString(encrypted, B64_FLAGS)
                editor.putString(datakey, dataval)
                logger.info("created pref item $datakey = $dataval")
            }
        }
        editor.apply()
    }

    private fun onToggleLockedClicked() {
        listAdapter.toggleUnlock()
        listAdapter.notifyDataSetChanged()
        updateToggleButton()
    }

    private fun updateToggleButton() {
        val res = if (listAdapter.unlocked) R.string.btn_toggle_lock else R.string.btn_toggle_unlock
        toggleBtn.setText(res)
    }

    companion object {
        private const val SAMPLE_PREFS_KEY = "protectedData"
    }
}
