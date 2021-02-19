/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.handler

import android.app.PendingIntent
import android.app.assist.AssistStructure
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.os.Build
import android.service.autofill.Dataset
import android.service.autofill.FillRequest
import android.service.autofill.FillResponse
import android.view.autofill.AutofillValue
import android.widget.RemoteViews
import androidx.annotation.RequiresApi
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.ext.getByStructure
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.feature.autofill.structure.parseStructure

/**
 * Class responsible for handling [FillRequest]s and returning [FillResponse]s.
 */
@RequiresApi(Build.VERSION_CODES.O)
internal class FillRequestHandler(
    private val context: Context,
    private val configuration: AutofillConfiguration
) {
    /**
     * Handles the given [FillRequest] and returns a matching [FillResponse] or `null` if this
     * request could not be handled.
     */
    suspend fun handle(request: FillRequest, forceUnlock: Boolean = false): FillResponse? {
        return handle(request.fillContexts.last().structure, forceUnlock)
    }

    /**
     * Handles a fill request for the given [AssistStructure] and returns a matching [FillResponse]
     * or `null` if the request could not be handled or the passed in [AssistStructure] is `null`.
     */
    @Suppress("ReturnCount")
    suspend fun handle(structure: AssistStructure?, forceUnlock: Boolean = false): FillResponse? {
        if (structure == null) {
            return null
        }

        val parsedStructure = parseStructure(context, structure) ?: return null
        val logins = configuration.storage.getByStructure(configuration.publicSuffixList, parsedStructure)

        if (logins.isEmpty()) {
            return null
        }

        return if (!configuration.lock.keepUnlocked() && !forceUnlock) {
            createAuthResponse(context, configuration, parsedStructure)
        } else {
            createLoginsResponse(context, parsedStructure, logins)
        }
    }
}

@RequiresApi(Build.VERSION_CODES.O)
private fun createAuthResponse(
    context: Context,
    configuration: AutofillConfiguration,
    parsedStructure: ParsedStructure
): FillResponse {
    val builder = FillResponse.Builder()

    val autofillIds = listOfNotNull(parsedStructure.usernameId, parsedStructure.passwordId)

    val authPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1).apply {
        setTextViewText(
            android.R.id.text1,
            context.getString(R.string.mozac_feature_autofill_popup_unlock_application, configuration.applicationName)
        )
    }

    val authIntent = Intent(context, configuration.unlockActivity)

    val intentSender: IntentSender = PendingIntent.getActivity(
        context,
        configuration.activityRequestCode,
        authIntent,
        PendingIntent.FLAG_CANCEL_CURRENT
    ).intentSender

    builder.setAuthentication(autofillIds.toTypedArray(), intentSender, authPresentation)

    return builder.build()
}

@RequiresApi(Build.VERSION_CODES.O)
private fun createLoginsResponse(
    context: Context,
    parsedStructure: ParsedStructure,
    logins: List<Login>
): FillResponse {
    val builder = FillResponse.Builder()

    logins.forEach { login ->
        val usernamePresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        usernamePresentation.setTextViewText(android.R.id.text1, login.usernamePresentationOrFallback(context))
        val passwordPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
        passwordPresentation.setTextViewText(android.R.id.text1, login.passwordPresentation(context))

        val dataset = Dataset.Builder()

        parsedStructure.usernameId?.let { id ->
            dataset.setValue(
                id,
                AutofillValue.forText(login.username),
                usernamePresentation
            )
        }

        parsedStructure.passwordId?.let { id ->
            dataset.setValue(
                id,
                AutofillValue.forText(login.password),
                passwordPresentation
            )
        }

        builder.addDataset(dataset.build())
    }

    return builder.build()
}

private fun Login.usernamePresentationOrFallback(context: Context): String {
    return if (username.isNotEmpty()) {
        username
    } else {
        context.getString(R.string.mozac_feature_autofill_popup_no_username)
    }
}

private fun Login.passwordPresentation(context: Context): String {
    return context.getString(
        R.string.mozac_feature_autofill_popup_password,
        usernamePresentationOrFallback(context)
    )
}
