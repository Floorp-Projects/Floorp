/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

/**
 * A value type representing a result of a "find in page" operation.
 *
 * @property activeMatchOrdinal the zero-based ordinal of the currently selected match.
 * @property numberOfMatches the match count
 * @property isDoneCounting true if the find operation has completed, otherwise false.
 */
data class FindResultState(val activeMatchOrdinal: Int, val numberOfMatches: Int, val isDoneCounting: Boolean)
