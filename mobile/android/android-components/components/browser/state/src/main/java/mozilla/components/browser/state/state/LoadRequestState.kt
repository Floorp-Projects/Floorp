/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * A value type holding information about an ongoing load request.
 *
 * @property url the URL being loaded
 * @property triggeredByRedirect True if the request is due to a redirect then true, otherwise false.
 * @property triggeredByUser True if the request is due to user interaction, otherwise false.
 */
data class LoadRequestState(
    val url: String,
    val triggeredByRedirect: Boolean,
    val triggeredByUser: Boolean,
)
