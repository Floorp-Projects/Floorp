/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import android.content.pm.PackageManager
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabsService.Relation
import androidx.browser.customtabs.CustomTabsSessionToken
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.CustomTabsAction
import mozilla.components.feature.customtabs.store.OriginRelationPair
import mozilla.components.feature.customtabs.store.ValidateRelationshipAction
import mozilla.components.feature.customtabs.store.VerificationStatus.FAILURE
import mozilla.components.feature.customtabs.store.VerificationStatus.PENDING
import mozilla.components.feature.customtabs.store.VerificationStatus.SUCCESS
import mozilla.components.feature.customtabs.verify.OriginVerifier
import mozilla.components.service.digitalassetlinks.RelationChecker

class OriginVerifierFeature(
    private val packageManager: PackageManager,
    private val relationChecker: RelationChecker,
    private val dispatch: (CustomTabsAction) -> Unit,
) {

    private var cachedVerifier: Triple<String, Int, OriginVerifier>? = null

    suspend fun verify(
        state: CustomTabState,
        token: CustomTabsSessionToken,
        @Relation relation: Int,
        origin: Uri,
    ): Boolean {
        val packageName = state.creatorPackageName ?: return false

        val existingRelation = state.relationships[OriginRelationPair(origin, relation)]
        return if (existingRelation == SUCCESS || existingRelation == FAILURE) {
            // Return if relation is already success or failure
            existingRelation == SUCCESS
        } else {
            val verifier = getVerifier(packageName, relation)
            dispatch(ValidateRelationshipAction(token, relation, origin, PENDING))

            val result = verifier.verifyOrigin(origin)
            val status = if (result) SUCCESS else FAILURE

            dispatch(ValidateRelationshipAction(token, relation, origin, status))
            result
        }
    }

    @VisibleForTesting
    internal fun getVerifier(packageName: String, @Relation relation: Int): OriginVerifier {
        cachedVerifier?.let {
            val (cachedPackage, cachedRelation, verifier) = it
            if (cachedPackage == packageName && cachedRelation == relation) {
                return verifier
            }
        }

        return OriginVerifier(packageName, relation, packageManager, relationChecker).also {
            cachedVerifier = Triple(packageName, relation, it)
        }
    }
}
