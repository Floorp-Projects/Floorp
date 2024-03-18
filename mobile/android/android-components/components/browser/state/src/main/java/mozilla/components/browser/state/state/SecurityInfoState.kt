/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * A value type holding security information for a Session.
 *
 * @property secure true if the tab is currently pointed to a URL with
 * a valid SSL certificate, otherwise false.
 * @property host domain for which the SSL certificate was issued.
 * @property issuer name of the certificate authority who issued the SSL certificate.
 */
data class SecurityInfoState(
    val secure: Boolean = false,
    val host: String = "",
    val issuer: String = "",
)
