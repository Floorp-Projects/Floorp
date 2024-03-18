/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.LabeledIntent
import android.os.Build
import android.os.Parcelable
import mozilla.components.support.utils.ext.queryIntentActivitiesCompat

/**
 * Modify the current intent to be used in an intent chooser excluding the current app.
 *
 * @param context Android context used for various system interactions.
 * @param title Title that will be displayed in the chooser.
 *
 * @return a new Intent object that you can hand to Context.startActivity() and related methods.
 */
fun Intent.createChooserExcludingCurrentApp(
    context: Context,
    title: CharSequence,
): Intent {
    val chooserIntent: Intent
    val resolveInfos = context.packageManager.queryIntentActivitiesCompat(this, 0).toHashSet()

    val excludedComponentNames = resolveInfos
        .map { it.activityInfo }
        .filter { it.packageName == context.packageName }
        .map { ComponentName(it.packageName, it.name) }

    // Starting with Android N we can use Intent.EXTRA_EXCLUDE_COMPONENTS to exclude components
    // other way we are constrained to use Intent.EXTRA_INITIAL_INTENTS.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
        chooserIntent = Intent.createChooser(this, title)
            .putExtra(
                Intent.EXTRA_EXCLUDE_COMPONENTS,
                excludedComponentNames.toTypedArray(),
            )
    } else {
        var targetIntents = resolveInfos
            .filterNot { it.activityInfo.packageName == context.packageName }
            .map { resolveInfo ->
                val activityInfo = resolveInfo.activityInfo
                val targetIntent = Intent(this).apply {
                    component = ComponentName(activityInfo.packageName, activityInfo.name)
                }
                LabeledIntent(
                    targetIntent,
                    activityInfo.packageName,
                    resolveInfo.labelRes,
                    resolveInfo.icon,
                )
            }

        // Sometimes on Android M and below an empty chooser is displayed, problem reported also here
        // https://issuetracker.google.com/issues/37085761
        // To fix that we are creating a chooser with an empty intent
        chooserIntent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Intent.createChooser(Intent(), title)
        } else {
            targetIntents = targetIntents.toMutableList()
            Intent.createChooser(targetIntents.removeAt(0), title)
        }
        chooserIntent.putExtra(
            Intent.EXTRA_INITIAL_INTENTS,
            targetIntents.toTypedArray<Parcelable>(),
        )
    }
    chooserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    return chooserIntent
}
