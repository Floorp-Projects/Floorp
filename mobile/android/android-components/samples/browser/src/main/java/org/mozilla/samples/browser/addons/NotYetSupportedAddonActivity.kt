/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.UnsupportedAddonsAdapter
import mozilla.components.feature.addons.ui.UnsupportedAddonsAdapterDelegate
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components

private const val LEARN_MORE_URL =
    "https://support.mozilla.org/kb/add-compatibility-firefox-preview"

/**
 * Activity for managing unsupported add-ons.
 */
class NotYetSupportedAddonActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val addons = requireNotNull(intent.getParcelableArrayListExtra<Addon>("add_ons"))

        supportFragmentManager
            .beginTransaction()
            .replace(R.id.container, NotYetSupportedAddonFragment.create(addons))
            .commit()
    }

    /**
     * Fragment for managing add-ons that are not yet supported by the browser.
     */
    class NotYetSupportedAddonFragment : Fragment(), UnsupportedAddonsAdapterDelegate {
        private lateinit var addons: List<Addon>
        private var adapter: UnsupportedAddonsAdapter? = null

        override fun onCreateView(
            inflater: LayoutInflater,
            container: ViewGroup?,
            savedInstanceState: Bundle?,
        ): View? {
            addons = requireNotNull(arguments?.getParcelableArrayList("add_ons"))
            return inflater.inflate(R.layout.fragment_not_yet_supported_addons, container, false)
        }

        override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
            super.onViewCreated(view, savedInstanceState)

            val context = requireContext()
            val recyclerView: RecyclerView = view.findViewById(R.id.unsupported_add_ons_list)
            adapter = UnsupportedAddonsAdapter(
                addonManager = context.components.addonManager,
                unsupportedAddonsAdapterDelegate = this@NotYetSupportedAddonFragment,
                addons = addons,
            )

            recyclerView.layoutManager = LinearLayoutManager(context)
            recyclerView.adapter = adapter

            view.findViewById<View>(R.id.learn_more_label).setOnClickListener {
                val intent = Intent(Intent.ACTION_VIEW).setData(Uri.parse(LEARN_MORE_URL))
                startActivity(intent)
            }
        }

        override fun onUninstallError(addonId: String, throwable: Throwable) {
            Toast.makeText(context, "Failed to remove add-on", Toast.LENGTH_SHORT).show()
        }

        override fun onUninstallSuccess() {
            Toast.makeText(context, "Successfully removed add-on", Toast.LENGTH_SHORT)
                .show()
            if (adapter?.itemCount == 0) {
                activity?.onBackPressed()
            }
        }

        companion object {
            /**
             * Create an [NotYetSupportedAddonFragment] with add_ons as a required parameter.
             */
            fun create(addons: ArrayList<Addon>) = NotYetSupportedAddonFragment().apply {
                arguments = Bundle().apply {
                    putParcelableArrayList("add_ons", addons)
                }
            }
        }
    }
}
