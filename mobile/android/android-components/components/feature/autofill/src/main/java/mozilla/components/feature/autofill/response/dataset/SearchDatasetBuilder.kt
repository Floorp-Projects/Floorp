/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.dataset

import android.annotation.SuppressLint
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.os.Build
import android.service.autofill.Dataset
import android.widget.inline.InlinePresentationSpec
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.handler.MAX_LOGINS
import mozilla.components.feature.autofill.structure.ParsedStructure

@RequiresApi(Build.VERSION_CODES.O)
internal data class SearchDatasetBuilder(
    val parsedStructure: ParsedStructure,
) : DatasetBuilder {

    @SuppressLint("NewApi")
    override fun build(
        context: Context,
        configuration: AutofillConfiguration,
        imeSpec: InlinePresentationSpec?,
    ): Dataset {
        val dataset = Dataset.Builder()

        val searchIntent = Intent(context, configuration.searchActivity)
        val searchPendingIntent = PendingIntent.getActivity(
            context,
            configuration.activityRequestCode + MAX_LOGINS,
            searchIntent,
            PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_CANCEL_CURRENT,
        )
        val intentSender: IntentSender = searchPendingIntent.intentSender

        val title = context.getString(
            R.string.mozac_feature_autofill_search_suggestions,
            configuration.applicationName,
        )

        val usernamePresentation = createViewPresentation(context, title)
        val passwordPresentation = createViewPresentation(context, title)

        val usernameInlinePresentation = createInlinePresentation(searchPendingIntent, imeSpec, title)
        val passwordInlinePresentation = createInlinePresentation(searchPendingIntent, imeSpec, title)

        parsedStructure.usernameId?.let { id ->
            dataset.setValue(
                id,
                null,
                usernamePresentation,
                usernameInlinePresentation,
            )
        }

        parsedStructure.passwordId?.let { id ->
            dataset.setValue(
                id,
                null,
                passwordPresentation,
                passwordInlinePresentation,
            )
        }

        dataset.setAuthentication(intentSender)

        return dataset.build()
    }
}
