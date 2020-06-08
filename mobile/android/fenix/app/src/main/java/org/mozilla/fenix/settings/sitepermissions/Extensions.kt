/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.view.View
import android.widget.TextView
import androidx.core.net.toUri
import androidx.core.text.HtmlCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.settings.PhoneFeature

/**
 * Try to reload a session if a session with the given [origin] it is found.
 * @param origin The origin the session to be reloaded.
 */
internal fun Components.tryReloadTabBy(origin: String) {
    val session = core.sessionManager.all.find { it.url.toUri().host == origin }
    useCases.sessionUseCases.reload(session)
}

internal fun initBlockedByAndroidView(phoneFeature: PhoneFeature, blockedByAndroidView: View) {
    val context = blockedByAndroidView.context
    if (!phoneFeature.isAndroidPermissionGranted(context)) {
        blockedByAndroidView.visibility = View.VISIBLE

        val descriptionLabel = blockedByAndroidView.findViewById<TextView>(R.id.blocked_by_android_feature_label)
        val descriptionText = context.getString(
            R.string.phone_feature_blocked_step_feature,
            phoneFeature.getLabel(context)
        )
        descriptionLabel.text = HtmlCompat.fromHtml(descriptionText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        val permissionsLabel = blockedByAndroidView.findViewById<TextView>(R.id.blocked_by_android_permissions_label)
        val permissionsText = context.getString(R.string.phone_feature_blocked_step_permissions)
        permissionsLabel.text = HtmlCompat.fromHtml(permissionsText, HtmlCompat.FROM_HTML_MODE_COMPACT)
    } else {
        blockedByAndroidView.visibility = View.GONE
    }
}
