/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.store

import android.net.Uri
import androidx.browser.customtabs.CustomTabsService.Relation
import androidx.browser.customtabs.CustomTabsSessionToken
import mozilla.components.lib.state.Action

sealed class CustomTabsAction : Action {
    abstract val token: CustomTabsSessionToken
}

/**
 * Saves the package name corresponding to a custom tab token.
 *
 * @property token Token of the custom tab.
 * @property packageName Package name of the app that created the custom tab.
 */
data class SaveCreatorPackageNameAction(
    override val token: CustomTabsSessionToken,
    val packageName: String,
) : CustomTabsAction()

/**
 * Marks the state of a custom tabs [Relation] verification.
 *
 * @property token Token of the custom tab to verify.
 * @property relation Relationship type to verify.
 * @property origin Origin to verify.
 * @property status State of the verification process.
 */
data class ValidateRelationshipAction(
    override val token: CustomTabsSessionToken,
    @Relation val relation: Int,
    val origin: Uri,
    val status: VerificationStatus,
) : CustomTabsAction()
