/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonPermissionsAdapter
import mozilla.components.feature.addons.ui.translateName
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
        title = addon.translateName(this)

        bindPermissions(addon)

        bindLearnMore()
    }

    private fun bindPermissions(addon: Addon) {
        val recyclerView = findViewById<RecyclerView>(R.id.add_ons_permissions)
        recyclerView.layoutManager = LinearLayoutManager(this)
        val sortedPermissions = addon.translatePermissions(this).sorted()
        recyclerView.adapter = AddonPermissionsAdapter(sortedPermissions)
    }

    private fun bindLearnMore() {
        findViewById<View>(R.id.learn_more_label).setOnClickListener(this)
    }

    override fun onClick(v: View?) {
        val intent =
            Intent(Intent.ACTION_VIEW).setData(Uri.parse(LEARN_MORE_URL))
        startActivity(intent)
    }
}
