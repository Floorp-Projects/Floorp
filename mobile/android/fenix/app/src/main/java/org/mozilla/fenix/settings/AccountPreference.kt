/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import kotlin.properties.Delegates

class AccountPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : Preference(context, attrs) {
    private var emailView: TextView? = null
    private var displayNameView: TextView? = null
    var displayName: String? by Delegates.observable<String?>(null) { _, _, new ->
        updateDisplayName(new)
    }

    var email: String? by Delegates.observable<String?>(null) { _, _, new ->
        new?.let { updateEmailText(it) }
    }

    init {
        layoutResource = R.layout.account_preference
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        displayNameView = holder.findViewById(R.id.displayName) as TextView
        emailView = holder.findViewById(R.id.email) as TextView

        updateDisplayName(displayName)
        // There is a potential for a race condition here. We might not have the user profile by the time we display
        // this field, in which case we won't have the email address (or the display name, but that we may just not have
        // at all even after fetching the profile). We don't hide the email field or change its text if email is missing
        // because in the layout a default value ("Firefox Account") is specified, which will be displayed instead.
        email?.let { emailView?.text = it }
    }

    private fun updateEmailText(email: String) {
        emailView?.text = email
    }

    private fun updateDisplayName(name: String?) {
        displayNameView?.text = name.orEmpty()
        displayNameView?.visibility = when (displayName.isNullOrEmpty()) {
            true -> View.GONE
            false -> View.VISIBLE
        }
    }
}
