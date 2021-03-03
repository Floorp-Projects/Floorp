/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.verify

import android.content.Context
import mozilla.components.service.digitalassetlinks.AndroidAssetFinder
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.local.StatementRelationChecker

/**
 * Helper to verify that a specific application is allowed to receive get the login credentials for
 * a specific domain.
 *
 * The verification is done through Digital Asset Links, which allow a domain to specify associated
 * apps and their signatures.
 * - https://developers.google.com/digital-asset-links/v1/getting-started
 * - https://github.com/google/digitalassetlinks/blob/master/well-known/details.md
 */
class CredentialAccessVerifier(
    private val checker: StatementRelationChecker,
    private val assetsFinder: AndroidAssetFinder = AndroidAssetFinder()
) {
    /**
     * Verifies and returns `true` if the application with [packageName] is allowed to receive
     * credentials for [domain] according to the hosted Digital Assets Links file. Returns `false`
     * otherwise. This method may also return `false` if a verification could not be performed,
     * e.g. the device is offline.
     */
    fun hasCredentialRelationship(
        context: Context,
        domain: String,
        packageName: String
    ): Boolean {
        val assets = assetsFinder.getAndroidAppAsset(packageName, context.packageManager).toList()

        // I was expecting us to need to verify all signatures here. But If I understand the usage
        // in `OriginVerifier` and the spec (see link in class comment) correctly then verifying one
        // certificate is enough to identify an app.
        val asset = assets.firstOrNull() ?: return false

        return checker.checkRelationship(
            AssetDescriptor.Web("https://$domain"),
            Relation.GET_LOGIN_CREDS,
            asset
        )
    }
}
