/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.fill

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.os.Build
import android.service.autofill.FillResponse
import android.widget.RemoteViews
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.feature.autofill.ui.AbstractAutofillUnlockActivity

internal data class AuthFillResponseBuilder(
    private val parsedStructure: ParsedStructure
) : FillResponseBuilder {

    @RequiresApi(Build.VERSION_CODES.O)
    override fun build(
        context: Context,
        configuration: AutofillConfiguration
    ): FillResponse {
        val builder = FillResponse.Builder()

        val autofillIds = listOfNotNull(parsedStructure.usernameId, parsedStructure.passwordId)

        val authPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1).apply {
            setTextViewText(
                android.R.id.text1,
                context.getString(
                    R.string.mozac_feature_autofill_popup_unlock_application,
                    configuration.applicationName
                )
            )
        }

        val authIntent = Intent(context, configuration.unlockActivity)
        authIntent.putExtra(
            AbstractAutofillUnlockActivity.EXTRA_PARSED_STRUCTURE,
            parsedStructure
        )

        val intentSender: IntentSender = PendingIntent.getActivity(
            context,
            configuration.activityRequestCode,
            authIntent,
            PendingIntent.FLAG_CANCEL_CURRENT or PendingIntent.FLAG_IMMUTABLE
        ).intentSender

        builder.setAuthentication(autofillIds.toTypedArray(), intentSender, authPresentation)

        return builder.build()
    }
}
