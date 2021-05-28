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
import android.widget.RemoteViews
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.handler.MAX_LOGINS
import mozilla.components.feature.autofill.structure.ParsedStructure

@RequiresApi(Build.VERSION_CODES.O)
internal data class SearchDatasetBuilder(
    val parsedStructure: ParsedStructure
) : DatasetBuilder {
    override fun build(context: Context, configuration: AutofillConfiguration): Dataset {
        val dataset = Dataset.Builder()

        val title = context.getString(
            R.string.mozac_feature_autofill_search_suggestions,
            configuration.applicationName
        )

        val usernamePresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        usernamePresentation.setTextViewText(android.R.id.text1, title)
        val passwordPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        passwordPresentation.setTextViewText(android.R.id.text1, title)

        parsedStructure.usernameId?.let { id ->
            dataset.setValue(
                id,
                null,
                usernamePresentation
            )
        }

        parsedStructure.passwordId?.let { id ->
            dataset.setValue(
                id,
                null,
                passwordPresentation
            )
        }

        val searchIntent = Intent(context, configuration.searchActivity)

        val intentSender: IntentSender = PendingIntent.getActivity(
            context,
            configuration.activityRequestCode + MAX_LOGINS,
            searchIntent,
            PendingIntent.FLAG_CANCEL_CURRENT
        ).intentSender

        dataset.setAuthentication(intentSender)

        return dataset.build()
    }
}
