/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address

import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.storage.Address
import org.mozilla.fenix.R

internal const val DEFAULT_COUNTRY = "US"

/**
 * Value type representing properties determined by the country used in an [Address].
 * This data is meant to mirror the data currently represented on desktop here:
 * https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/addressmetadata/addressReferences.js
 *
 * This can be expanded to included things like a list of applicable states/provinces per country
 * or the names that should be used for each form field.
 *
 * Note: Most properties here need to be kept in sync with the data in the above desktop
 * address reference file in order to prevent duplications when sync is enabled. There are
 * ongoing conversations about how best to share that data cross-platform, if at all.
 * Some more detail: https://bugzilla.mozilla.org/show_bug.cgi?id=1769809
 *
 * Exceptions: [displayName] is a local property and stop-gap to a more robust solution.
 *
 * @property countryCode The country code used to lookup the address data. Should match desktop entries.
 * @property displayName The name to display when selected.
 * @property subregionTitleResource The string resource for the subregion title.
 * @property subregions THe list of subregions.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal data class Country(
    val countryCode: String,
    val displayName: String,
    @StringRes val subregionTitleResource: Int,
    val subregions: List<String>,
)

internal object AddressUtils {
    /**
     * The current list of supported countries.
     */
    val countries = mapOf(
        "CA" to Country(
            countryCode = "CA",
            displayName = "Canada",
            subregionTitleResource = R.string.addresses_province,
            subregions = Subregions.CA,
        ),
        "US" to Country(
            countryCode = "US",
            displayName = "United States",
            subregionTitleResource = R.string.addresses_state,
            subregions = Subregions.US,
        ),
    )

    /**
     * Get the country code associated with a [Country.displayName], or the [DEFAULT_COUNTRY] code
     * if the display name is not supported.
     */
    fun getCountryCode(displayName: String) = countries.values.find {
        it.displayName == displayName
    }?.countryCode ?: DEFAULT_COUNTRY
}

/**
 * Convert a [Country.displayName] to the associated country code.
 */
fun String.toCountryCode() = AddressUtils.getCountryCode(this)

private object Subregions {
    // This data is meant to mirror the data currently represented on desktop here:
    // https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/addressmetadata/addressReferences.js
    val CA = listOf(
        "Alberta",
        "British Columbia",
        "Manitoba",
        "New Brunswick",
        "Newfoundland and Labrador",
        "Northwest Territories",
        "Nova Scotia",
        "Nunavut",
        "Ontario",
        "Prince Edward Island",
        "Quebec",
        "Saskatchewan",
        "Yukon",
    )

    // This data is meant to mirror the data currently represented on desktop here:
    // https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/addressmetadata/addressReferences.js
    val US = listOf(
        "Alabama",
        "Alaska",
        "American Samoa",
        "Arizona",
        "Arkansas",
        "Armed Forces (AA)",
        "Armed Forces (AE)",
        "Armed Forces (AP)",
        "California",
        "Colorado",
        "Connecticut",
        "Delaware",
        "District of Columbia",
        "Florida",
        "Georgia",
        "Guam",
        "Hawaii",
        "Idaho",
        "Illinois",
        "Indiana",
        "Iowa",
        "Kansas",
        "Kentucky",
        "Louisiana",
        "Maine",
        "Marshall Islands",
        "Maryland",
        "Massachusetts",
        "Michigan",
        "Micronesia",
        "Minnesota",
        "Mississippi",
        "Missouri",
        "Montana",
        "Nebraska",
        "Nevada",
        "New Hampshire",
        "New Jersey",
        "New Mexico",
        "New York",
        "North Carolina",
        "North Dakota",
        "Northern Mariana Islands",
        "Ohio",
        "Oklahoma",
        "Oregon",
        "Palau",
        "Pennsylvania",
        "Puerto Rico",
        "Rhode Island",
        "South Carolina",
        "South Dakota",
        "Tennessee",
        "Texas",
        "Utah",
        "Vermont",
        "Virgin Islands",
        "Virginia",
        "Washington",
        "West Virginia",
        "Wisconsin",
        "Wyoming",
    )
}
