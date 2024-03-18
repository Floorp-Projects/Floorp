/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The language container for presenting language information to the user.
 *
 * @property code The BCP 47 code that represents the language.
 * @property localizedDisplayName The translations engine localized display name of the language.
 */
data class Language(
    val code: String,
    val localizedDisplayName: String? = null,
)
