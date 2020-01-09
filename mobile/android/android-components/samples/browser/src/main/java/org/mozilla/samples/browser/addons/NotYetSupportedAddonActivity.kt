/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.Addon
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components

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
    class NotYetSupportedAddonFragment : Fragment() {
        private lateinit var addons: List<Addon>

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
            addons = requireNotNull(arguments?.getParcelableArrayList("add_ons"))
            return inflater.inflate(R.layout.fragment_not_yet_supported_addons, container, false)
        }

        override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
            super.onViewCreated(view, savedInstanceState)

            val recyclerView: RecyclerView = view.findViewById(R.id.unsupported_add_ons_list)
            recyclerView.layoutManager = LinearLayoutManager(requireContext())
            recyclerView.adapter = UnsupportedAddonsAdapter(addons)
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

    /**
     * An adapter for displaying unsupported add-on items.
     */
    class UnsupportedAddonsAdapter(
        private val addons: List<Addon>
    ) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
        override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
            holder as UnsupportedAddonViewHolder
            val context = holder.itemView.context
            val addon = addons[position]

            holder.titleView.text =
                if (addon.translatableName.isNotEmpty()) {
                    addon.translatableName.translate()
                } else {
                    addon.id
                }

            holder.removeButton.setOnClickListener {
                context.components.addonManager.uninstallAddon(addon,
                    onSuccess = {
                        Toast.makeText(context, "Successfully removed add-on", Toast.LENGTH_SHORT).show()
                    },
                    onError = { _, _ ->
                        Toast.makeText(context, "Failed to remove add-on", Toast.LENGTH_SHORT).show()
                    })
            }
        }

        override fun getItemCount(): Int {
            return addons.size
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.add_ons_unsupported_item, parent, false)

            val iconView = view.findViewById<ImageView>(R.id.add_on_icon)
            val titleView = view.findViewById<TextView>(R.id.add_on_name)
            val removeButton = view.findViewById<ImageButton>(R.id.add_on_remove_button)

            return UnsupportedAddonViewHolder(view, iconView, titleView, removeButton)
        }

        /**
         * A view holder for displaying unsupported add-on items.
         */
        class UnsupportedAddonViewHolder(
            view: View,
            val iconView: ImageView,
            val titleView: TextView,
            val removeButton: ImageButton
        ) : RecyclerView.ViewHolder(view)
    }
}
