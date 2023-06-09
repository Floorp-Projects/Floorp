/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.identitycredential

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * Represents an Identity credential provider:
 * @property id An identifier for this [Provider].
 * @property icon an icon of the provider, normally the logo of the brand.
 * @property name @property name The name of this [Provider].
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class Provider(
    val id: Int,
    val icon: String?,
    val name: String,
) : Parcelable
