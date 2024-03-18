/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.identitycredential

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * Represents an Identity credential account:
 * @property id An identifier for this [Account].
 * @property email The email associated to this [Account].
 * @property name The name of this [Account].
 * @property icon An icon for the [Account], normally the profile picture
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class Account(
    val id: Int,
    val email: String,
    val name: String,
    val icon: String?,
) : Parcelable
