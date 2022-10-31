/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.mediaquery

/**
 * A simple data class used to suggest to page content that the user prefers a particular color
 * scheme.
 */
sealed class PreferredColorScheme {
    companion object

    object Light : PreferredColorScheme()
    object Dark : PreferredColorScheme()
    object System : PreferredColorScheme()
}
