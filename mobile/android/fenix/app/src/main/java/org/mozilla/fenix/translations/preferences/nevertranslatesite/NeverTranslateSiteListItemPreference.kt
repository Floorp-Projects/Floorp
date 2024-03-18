/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.nevertranslatesite

/**
 * NeverTranslateSiteListItemPreference that will appear on [NeverTranslateSitePreferenceFragment] screens.
 *
 * @property websiteUrl The text that will appear on the item list.
 */
data class NeverTranslateSiteListItemPreference(val websiteUrl: String)
