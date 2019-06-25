/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Value type that represents the state of the content displayed in a [SessionState].
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
