/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.store

import android.net.Uri
import androidx.browser.customtabs.CustomTabsService
import androidx.browser.customtabs.CustomTabsSessionToken
import mozilla.components.lib.state.State

/**
 * Value type that represents the custom tabs state
 * accessible from both the service and activity.
 */
data class CustomTabsServiceState(
    val tabs: Map<CustomTabsSessionToken, CustomTabState> = emptyMap(),
) : State

/**
 * Value type that represents the state of a single custom tab
 * accessible from both the service and activity.
 *
 * This data is meant to supplement [mozilla.components.browser.session.tab.CustomTabConfig],
 * not replace it. It only contains data that the service also needs to work with.
 *
 * @property creatorPackageName Package name of the app that created the custom tab.
 * @property relationships Map of origin and relationship type to current verification state.
 */
data class CustomTabState(
    val creatorPackageName: String? = null,
    val relationships: Map<OriginRelationPair, VerificationStatus> = emptyMap(),
)

/**
 * Pair of origin and relation type used as key in [CustomTabState.relationships].
 *
 * @property origin URL that contains only the scheme, host, and port.
 * https://html.spec.whatwg.org/multipage/origin.html#concept-origin
 * @property relation Enum that indicates the relation type.
 */
data class OriginRelationPair(
    val origin: Uri,
    @CustomTabsService.Relation val relation: Int,
)

/**
 * Different states of Digital Asset Link verification.
 */
enum class VerificationStatus {
    /**
     * Indicates verification has started and hasn't returned yet.
     *
     * To avoid flashing the toolbar, we choose to hide it when a Digital Asset Link is being verified.
     * We only show the toolbar when the verification fails, or an origin never requested to be verified.
     */
    PENDING,

    /**
     * Indicates that verification has completed and the link was verified.
     */
    SUCCESS,

    /**
     * Indicates that verification has completed and the link was invalid.
     */
    FAILURE,
}
