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
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.Addon
import org.mozilla.samples.browser.R

private const val LEARN_MORE_URL =
    "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions"

/**
 * An activity to show the permissions of an add-on.
 */
class PermissionsDetailsActivity : AppCompatActivity(), View.OnClickListener {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_add_on_permissions)
        val addon = requireNotNull(intent.getParcelableExtra<Addon>("add_on"))
        title = addon.translatableName.translate()

        bindPermissions(addon)

        bindLearnMore()
    }

    private fun bindPermissions(addon: Addon) {
        val recyclerView = findViewById<RecyclerView>(R.id.add_ons_permissions)
        recyclerView.layoutManager = LinearLayoutManager(this)
        val sortedPermissions = addon.translatePermissions().map { stringId ->
            getString(stringId)
        }.sorted()
        recyclerView.adapter = PermissionsAdapter(sortedPermissions)
    }

    private fun bindLearnMore() {
        findViewById<View>(R.id.learn_more_label).setOnClickListener(this)
    }

    /**
     * An adapter for displaying the permissions of an add-on.
     */
    class PermissionsAdapter(
        private val permissions: List<String>
    ) :
        RecyclerView.Adapter<PermissionViewHolder>() {
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): PermissionViewHolder {
            val context = parent.context
            val inflater = LayoutInflater.from(context)
            val view = inflater.inflate(R.layout.add_ons_permission_item, parent, false)
            val titleView = view.findViewById<TextView>(R.id.permission)
            return PermissionViewHolder(
                view,
                titleView
            )
        }

        override fun getItemCount() = permissions.size

        override fun onBindViewHolder(holder: PermissionViewHolder, position: Int) {
            val permission = permissions[position]
            holder.textView.text = permission
        }
    }

    /**
     * A view holder for displaying the permissions of an add-on.
     */
    class PermissionViewHolder(
        val view: View,
        val textView: TextView
    ) : RecyclerView.ViewHolder(view)

    override fun onClick(v: View?) {
        val intent =
            Intent(Intent.ACTION_VIEW).setData(Uri.parse(LEARN_MORE_URL))
        startActivity(intent)
    }
}
