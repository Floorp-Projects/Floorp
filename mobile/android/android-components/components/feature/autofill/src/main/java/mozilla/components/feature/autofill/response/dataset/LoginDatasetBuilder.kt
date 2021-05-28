/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.dataset

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.os.Build
import android.service.autofill.Dataset
import android.view.autofill.AutofillValue
import android.widget.RemoteViews
import androidx.annotation.RequiresApi
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.handler.EXTRA_LOGIN_ID
import mozilla.components.feature.autofill.structure.ParsedStructure

@RequiresApi(Build.VERSION_CODES.O)
internal data class LoginDatasetBuilder(
    val parsedStructure: ParsedStructure,
    val login: Login,
    val needsConfirmation: Boolean,
    val requestOffset: Int = 0
) : DatasetBuilder {
    override fun build(
        context: Context,
        configuration: AutofillConfiguration
    ): Dataset {
        val dataset = Dataset.Builder()

        val usernamePresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        usernamePresentation.setTextViewText(android.R.id.text1, login.usernamePresentationOrFallback(context))
        val passwordPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        passwordPresentation.setTextViewText(android.R.id.text1, login.passwordPresentation(context))

        parsedStructure.usernameId?.let { id ->
            dataset.setValue(
                id,
                if (needsConfirmation) null else AutofillValue.forText(login.username),
                usernamePresentation
            )
        }

        parsedStructure.passwordId?.let { id ->
            dataset.setValue(
                id,
                if (needsConfirmation) null else AutofillValue.forText(login.password),
                passwordPresentation
            )
        }

        if (needsConfirmation) {
            val confirmIntent = Intent(context, configuration.confirmActivity)
            confirmIntent.putExtra(EXTRA_LOGIN_ID, login.guid)

            val intentSender: IntentSender = PendingIntent.getActivity(
                context,
                configuration.activityRequestCode + requestOffset,
                confirmIntent,
                PendingIntent.FLAG_CANCEL_CURRENT
            ).intentSender

            dataset.setAuthentication(intentSender)
        }

        return dataset.build()
    }
}

internal fun Login.usernamePresentationOrFallback(context: Context): String {
    return if (username.isNotEmpty()) {
        username
    } else {
        context.getString(mozilla.components.feature.autofill.R.string.mozac_feature_autofill_popup_no_username)
    }
}

private fun Login.passwordPresentation(context: Context): String {
    return context.getString(
        mozilla.components.feature.autofill.R.string.mozac_feature_autofill_popup_password,
        usernamePresentationOrFallback(context)
    )
}
