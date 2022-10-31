/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.dataset

import android.annotation.SuppressLint
import android.app.PendingIntent
import android.app.slice.Slice
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.graphics.BlendMode
import android.graphics.drawable.Icon
import android.os.Build
import android.service.autofill.Dataset
import android.service.autofill.InlinePresentation
import android.text.TextUtils
import android.view.autofill.AutofillId
import android.view.autofill.AutofillValue
import android.widget.RemoteViews
import android.widget.inline.InlinePresentationSpec
import androidx.annotation.RequiresApi
import androidx.autofill.inline.UiVersions
import androidx.autofill.inline.v1.InlineSuggestionUi
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.handler.EXTRA_LOGIN_ID
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.support.utils.PendingIntentUtils

@RequiresApi(Build.VERSION_CODES.O)
internal data class LoginDatasetBuilder(
    val parsedStructure: ParsedStructure,
    val login: Login,
    val needsConfirmation: Boolean,
    val requestOffset: Int = 0,
) : DatasetBuilder {

    @SuppressLint("NewApi")
    override fun build(
        context: Context,
        configuration: AutofillConfiguration,
        imeSpec: InlinePresentationSpec?,
    ): Dataset {
        val dataset = Dataset.Builder()

        val pendingIntent = PendingIntent.getActivity(
            context,
            0,
            Intent(),
            PendingIntentUtils.defaultFlags or PendingIntent.FLAG_CANCEL_CURRENT,
        )

        val usernameText = login.usernamePresentationOrFallback(context)
        val passwordText = login.passwordPresentation(context)

        val usernamePresentation = createViewPresentation(context, usernameText)
        val passwordPresentation = createViewPresentation(context, passwordText)

        val usernameInlinePresentation = createInlinePresentation(pendingIntent, imeSpec, usernameText)
        val passwordInlinePresentation = createInlinePresentation(pendingIntent, imeSpec, passwordText)

        parsedStructure.usernameId?.let { id ->
            dataset.setValue(
                id,
                if (needsConfirmation) null else AutofillValue.forText(login.username),
                usernamePresentation,
                usernameInlinePresentation,
            )
        }

        parsedStructure.passwordId?.let { id ->
            dataset.setValue(
                id,
                if (needsConfirmation) null else AutofillValue.forText(login.password),
                passwordPresentation,
                passwordInlinePresentation,
            )
        }

        if (needsConfirmation) {
            val confirmIntent = Intent(context, configuration.confirmActivity)
            confirmIntent.putExtra(EXTRA_LOGIN_ID, login.guid)

            val intentSender: IntentSender = PendingIntent.getActivity(
                context,
                configuration.activityRequestCode + requestOffset,
                confirmIntent,
                PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_CANCEL_CURRENT,
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
        usernamePresentationOrFallback(context),
    )
}

internal fun createViewPresentation(context: Context, title: String): RemoteViews {
    val viewPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1)
    viewPresentation.setTextViewText(android.R.id.text1, title)

    return viewPresentation
}

internal fun createInlinePresentation(
    pendingIntent: PendingIntent,
    imeSpec: InlinePresentationSpec?,
    title: String,
    icon: Icon? = null,
): InlinePresentation? {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && imeSpec != null &&
        canUseInlineSuggestions(imeSpec)
    ) {
        return InlinePresentation(
            createSlice(title, attribution = pendingIntent, startIcon = icon),
            imeSpec,
            false,
        )
    }
    return null
}

@SuppressLint("RestrictedApi")
@RequiresApi(Build.VERSION_CODES.R)
internal fun createSlice(
    title: CharSequence,
    subtitle: CharSequence = "",
    startIcon: Icon? = null,
    endIcon: Icon? = null,
    contentDescription: CharSequence = "",
    attribution: PendingIntent,
): Slice {
    // Build the content for the v1 UI.
    val builder = InlineSuggestionUi.newContentBuilder(attribution)
        .setContentDescription(contentDescription)
    if (!TextUtils.isEmpty(title)) {
        builder.setTitle(title)
    }
    if (!TextUtils.isEmpty(subtitle)) {
        builder.setSubtitle(subtitle)
    }
    if (startIcon != null) {
        startIcon.setTintBlendMode(BlendMode.DST)
        builder.setStartIcon(startIcon)
    }
    if (endIcon != null) {
        builder.setEndIcon(endIcon)
    }
    return builder.build().slice
}

@RequiresApi(Build.VERSION_CODES.R)
internal fun canUseInlineSuggestions(imeSpec: InlinePresentationSpec): Boolean {
    return UiVersions.getVersions(imeSpec.style).contains(UiVersions.INLINE_UI_VERSION_1)
}

@RequiresApi(Build.VERSION_CODES.O)
internal fun Dataset.Builder.setValue(
    id: AutofillId,
    value: AutofillValue?,
    presentation: RemoteViews,
    inlinePresentation: InlinePresentation? = null,
): Dataset.Builder {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && inlinePresentation != null) {
        this.setValue(id, value, presentation, inlinePresentation)
    } else {
        this.setValue(id, value, presentation)
    }
}
