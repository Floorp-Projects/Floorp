/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Value type that represents the state of reader mode/view.
 *
 * @property readerable whether or not the current page can be transformed to
 * be displayed in a reader view.
 * @property active whether or not reader view is active.
 * @property checkRequired whether or not a readerable check is required for the
 * current page.
 * @property connectRequired whether or not a new connection to the reader view
 * content script is required.
 * @property baseUrl the base URL of the reader view extension page.
 * @property activeUrl the URL of the page currently displayed in reader view.
 */
data class ReaderState(
    val readerable: Boolean = false,
    val active: Boolean = false,
    val checkRequired: Boolean = false,
    val connectRequired: Boolean = false,
    val baseUrl: String? = null,
    val activeUrl: String? = null,
)
