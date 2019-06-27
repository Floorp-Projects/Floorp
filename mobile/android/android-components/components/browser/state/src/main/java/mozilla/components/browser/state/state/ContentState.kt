/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Value type that represents the state of the content within a [SessionState].
 *
 * @property url the loading or loaded URL.
 * @property private whether or not the session is private.
 * @property title the title of the current page.
 * @property progress the loading progress of the current page.
 * @property searchTerms the last used search terms, or an empty string if no
 * search was executed for this session.
 * @property securityInfo the security information as [SecurityInfoState],
 * describing whether or not the current session is for a secure URL, as well
 * as the host and SSL certificate authority.
 */
data class ContentState(
    val url: String,
    val private: Boolean = false,
    val title: String = "",
    val progress: Int = 0,
    val loading: Boolean = false,
    val searchTerms: String = "",
    val securityInfo: SecurityInfoState = SecurityInfoState()
)
