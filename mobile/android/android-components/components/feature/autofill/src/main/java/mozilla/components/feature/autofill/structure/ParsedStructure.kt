/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.structure

import android.app.assist.AssistStructure
import android.content.Context
import android.os.Build
import android.view.autofill.AutofillId
import androidx.annotation.RequiresApi

/**
 * Parsed structure from an autofill request.
 *
 * Originally implemented in Lockwise:
 * https://github.com/mozilla-lockwise/lockwise-android/blob/d3c0511f73c34e8759e1bb597f2d3dc9bcc146f0/app/src/main/java/mozilla/lockbox/autofill/ParsedStructure.kt#L52
 */
@RequiresApi(Build.VERSION_CODES.O)
internal data class ParsedStructure(
    val usernameId: AutofillId? = null,
    val passwordId: AutofillId? = null,
    val webDomain: String? = null,
    val packageName: String
)

@RequiresApi(Build.VERSION_CODES.O)
internal fun parseStructure(context: Context, structure: AssistStructure): ParsedStructure? {
    val activityPackageName = structure.activityComponent.packageName
    if (context.packageName == activityPackageName) {
        // We do not autofill our own activities. Browser content will be auto-filled by Gecko.
        return null
    }

    val nodeNavigator = ViewNodeNavigator(structure, activityPackageName)
    val parsedStructure = ParsedStructureBuilder(nodeNavigator).build()

    if (parsedStructure.passwordId == null && parsedStructure.usernameId == null) {
        // If we didn't find any password or username fields then there's nothing to autofill for us.
        return null
    }

    return parsedStructure
}
