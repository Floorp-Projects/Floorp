/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.launch
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.bitmapForUrl
import org.mozilla.fenix.settings.requirePreference

class AccountUiView(
    fragment: PreferenceFragmentCompat,
    private val accountManager: FxaAccountManager,
    private val httpClient: Client,
    private val updateFxASyncOverrideMenu: () -> Unit
) {

    private val lifecycleScope = fragment.viewLifecycleOwner.lifecycleScope
    private val preferenceSignIn =
        fragment.requirePreference<Preference>(R.string.pref_key_sign_in)
    private val preferenceFirefoxAccount =
        fragment.requirePreference<AccountPreference>(R.string.pref_key_account)
    private val preferenceFirefoxAccountAuthError =
        fragment.requirePreference<AccountAuthErrorPreference>(R.string.pref_key_account_auth_error)
    private val accountPreferenceCategory =
        fragment.requirePreference<PreferenceCategory>(R.string.pref_key_account_category)

    /**
     * Updates the UI to reflect current account state.
     * Possible conditions are logged-in without problems, logged-out, and logged-in but needs to re-authenticate.
     */
    fun updateAccountUIState(context: Context, profile: Profile?) {
        val account = accountManager.authenticatedAccount()

        updateFxASyncOverrideMenu()

        // Signed-in, no problems.
        if (account != null && !accountManager.accountNeedsReauth()) {
            preferenceSignIn.isVisible = false

            profile?.avatar?.url?.let { avatarUrl ->
                lifecycleScope.launch {
                    val roundedDrawable = toRoundedDrawable(avatarUrl, context)
                    preferenceFirefoxAccount.icon =
                        roundedDrawable ?: AppCompatResources.getDrawable(
                            context,
                            R.drawable.ic_account
                        )
                }
            }

            preferenceSignIn.onPreferenceClickListener = null
            preferenceFirefoxAccountAuthError.isVisible = false
            preferenceFirefoxAccount.isVisible = true
            accountPreferenceCategory.isVisible = true

            preferenceFirefoxAccount.displayName = profile?.displayName
            preferenceFirefoxAccount.email = profile?.email

            // Signed-in, need to re-authenticate.
        } else if (account != null && accountManager.accountNeedsReauth()) {
            preferenceFirefoxAccount.isVisible = false
            preferenceFirefoxAccountAuthError.isVisible = true
            accountPreferenceCategory.isVisible = true

            preferenceSignIn.isVisible = false
            preferenceSignIn.onPreferenceClickListener = null

            preferenceFirefoxAccountAuthError.email = profile?.email

            // Signed-out.
        } else {
            preferenceSignIn.isVisible = true
            preferenceFirefoxAccount.isVisible = false
            preferenceFirefoxAccountAuthError.isVisible = false
            accountPreferenceCategory.isVisible = false
        }
    }

    /**
     * Gets a rounded drawable from a URL if possible, else null.
     */
    private suspend fun toRoundedDrawable(
        url: String,
        context: Context
    ) = bitmapForUrl(url, httpClient)?.let { bitmap ->
        RoundedBitmapDrawableFactory.create(context.resources, bitmap).apply {
            isCircular = true
            setAntiAlias(true)
        }
    }
}
