/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.app.PendingIntent
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.LabeledIntent
import android.os.Build
import android.os.Parcelable
import org.mozilla.focus.ext.queryIntentActivitiesCompat

object IntentUtils {

    /**
     * Since Android 12 we need to set PendingIntent mutability explicitly, but Android 6 can be the minimum version
     * This additional requirement improves your app's security.
     * FLAG_IMMUTABLE -> Flag indicating that the created PendingIntent should be immutable.
     */
    val defaultIntentPendingFlags
        get() = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            PendingIntent.FLAG_IMMUTABLE
        } else {
            0 // No flags. Default behavior.
        }

    /**
     * Method for creating an intent chooser but without the current app
     */
    fun getIntentChooser(
        context: Context,
        intent: Intent,
        chooserTitle: CharSequence? = null,
    ): Intent {
        val chooserIntent: Intent
        val resolveInfos = context.packageManager.queryIntentActivitiesCompat(intent, 0).toHashSet()

        val excludedComponentNames = resolveInfos
            .map { it.activityInfo }
            .filter { it.packageName == context.packageName }
            .map { ComponentName(it.packageName, it.name) }

        // Starting with Android N we can use Intent.EXTRA_EXCLUDE_COMPONENTS to exclude components
        // other way we are constrained to use Intent.EXTRA_INITIAL_INTENTS.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            chooserIntent = Intent.createChooser(intent, chooserTitle)
                .putExtra(Intent.EXTRA_EXCLUDE_COMPONENTS, excludedComponentNames.toTypedArray())
        } else {
            var targetIntents = resolveInfos
                .filterNot { it.activityInfo.packageName == context.packageName }
                .map { resolveInfo ->
                    val activityInfo = resolveInfo.activityInfo
                    val targetIntent = Intent(intent).apply {
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
                Intent.createChooser(Intent(), chooserTitle)
            } else {
                targetIntents = targetIntents.toMutableList()
                Intent.createChooser(targetIntents.removeAt(0), chooserTitle)
            }
            chooserIntent.putExtra(
                Intent.EXTRA_INITIAL_INTENTS,
                targetIntents.toTypedArray<Parcelable>(),
            )
        }
        return chooserIntent
    }
}
