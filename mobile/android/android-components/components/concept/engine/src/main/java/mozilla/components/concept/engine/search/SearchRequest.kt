/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.search

/**
 * Value type that represents a request for showing a search to the user.
 */
data class SearchRequest(val isPrivate: Boolean, val query: String)
