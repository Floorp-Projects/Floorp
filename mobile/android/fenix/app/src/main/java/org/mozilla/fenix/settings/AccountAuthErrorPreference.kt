/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.core.view.isGone
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import kotlin.properties.Delegates

class AccountAuthErrorPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    attributeSetId: Int = android.R.attr.preferenceStyle
) : Preference(context, attrs, attributeSetId) {
    private var emailView: TextView? = null
    var email: String? by Delegates.observable<String?>(null) { _, _, new -> updateEmailView(new) }

    init {
        layoutResource = R.layout.account_auth_error_preference
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        emailView = holder.findViewById(R.id.email) as TextView
        updateEmailView(email)
    }

    private fun updateEmailView(email: String?) {
        emailView?.text = email
        emailView?.isGone = email.isNullOrEmpty()
    }
}
