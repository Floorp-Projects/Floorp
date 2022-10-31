/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.structure

import android.content.Context
import android.os.Build
import android.os.Parcel
import android.os.Parcelable
import android.os.Parcelable.Creator
import android.view.autofill.AutofillId
import androidx.annotation.RequiresApi
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.utils.Browsers

/**
 * Parsed structure from an autofill request.
 *
 * Originally implemented in Lockwise:
 * https://github.com/mozilla-lockwise/lockwise-android/blob/d3c0511f73c34e8759e1bb597f2d3dc9bcc146f0/app/src/main/java/mozilla/lockbox/autofill/ParsedStructure.kt#L52
 */
@RequiresApi(Build.VERSION_CODES.O)
data class ParsedStructure(
    val usernameId: AutofillId? = null,
    val passwordId: AutofillId? = null,
    val webDomain: String? = null,
    val packageName: String,
) : Parcelable {
    constructor(parcel: Parcel) : this(
        parcel.readParcelable(AutofillId::class.java.classLoader),
        parcel.readParcelable(AutofillId::class.java.classLoader),
        parcel.readString(),
        parcel.readString() ?: "",
    )

    override fun writeToParcel(parcel: Parcel, flags: Int) {
        parcel.writeParcelable(usernameId, flags)
        parcel.writeParcelable(passwordId, flags)
        parcel.writeString(webDomain)
        parcel.writeString(packageName)
    }

    override fun describeContents(): Int {
        return 0
    }

    /**
     * Create instances of [ParsedStructure] from a [Parcel].
     */
    companion object CREATOR : Creator<ParsedStructure> {
        override fun createFromParcel(parcel: Parcel): ParsedStructure {
            return ParsedStructure(parcel)
        }

        override fun newArray(size: Int): Array<ParsedStructure?> {
            return arrayOfNulls(size)
        }
    }
}

/**
 * Try to find a domain in the [ParsedStructure] for looking up logins. This is either a "web domain"
 * for web content the third-party app is displaying (e.g. in a WebView) or the package name of the
 * application transformed into a domain. In any case the [publicSuffixList] will be used to turn
 * the domain into a "base" domain (public suffix + 1) before returning.
 */
internal suspend fun ParsedStructure.getLookupDomain(publicSuffixList: PublicSuffixList): String {
    println("Lookup: webDomain=$webDomain, packageName=$packageName")
    val domain = if (webDomain != null && Browsers.isBrowser(packageName)) {
        // If the application we are auto-filling is a known browser and it provided a webDomain
        // for the content it is displaying then we try to autofill for that.
        webDomain
    } else {
        // We reverse the package name in the hope that this will resemble a domain name. This is
        // of course fragile. So we want to find better mechanisms in the future (e.g. looking up
        // what URLs the application registers intent handlers for).
        packageName.split('.').asReversed().joinToString(".")
    }

    return publicSuffixList.getPublicSuffixPlusOne(domain).await() ?: domain
}

@RequiresApi(Build.VERSION_CODES.O)
internal fun parseStructure(context: Context, structure: RawStructure): ParsedStructure? {
    val activityPackageName = structure.activityPackageName
    if (context.packageName == activityPackageName) {
        // We do not autofill our own activities. Browser content will be auto-filled by Gecko.
        return null
    }

    val nodeNavigator = structure.createNavigator()
    val parsedStructure = ParsedStructureBuilder(nodeNavigator).build()

    if (parsedStructure.passwordId == null && parsedStructure.usernameId == null) {
        // If we didn't find any password or username fields then there's nothing to autofill for us.
        return null
    }

    return parsedStructure
}
