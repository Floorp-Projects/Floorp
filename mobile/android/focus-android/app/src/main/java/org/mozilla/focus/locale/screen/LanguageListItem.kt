/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

/**
 * Data class for the LanguageListItem that goes to the compose ListView
 *
 * @param language item to be display in ListView
 * @param onClick Callback when the user taps on Language Item
 */
data class LanguageListItem(
    val language: Language,
    val onClick: (String) -> Unit,
)
