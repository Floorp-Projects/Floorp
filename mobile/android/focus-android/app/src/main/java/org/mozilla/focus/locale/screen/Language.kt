/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

/**
 * Data class for Language that comes from the LanguageStorage
 *
 * @param displayName of the Language the will be shown in the GUI
 * @param tag of the Language that will be saved in SharePref if the element is selected
 * @param index of the Language in the list .It is used for auto-scrolling to the current selected
 * Language
 */
data class Language(var displayName: String?, val tag: String, val index: Int)
