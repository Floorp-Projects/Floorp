/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.fill

import android.annotation.SuppressLint
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.graphics.drawable.Icon
import android.os.Build
import android.os.Parcel
import android.service.autofill.FillResponse
import android.service.autofill.InlinePresentation
import android.view.autofill.AutofillId
import android.widget.RemoteViews
import android.widget.inline.InlinePresentationSpec
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.response.dataset.createInlinePresentation
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.feature.autofill.ui.AbstractAutofillUnlockActivity

internal data class AuthFillResponseBuilder(
    private val parsedStructure: ParsedStructure,
    private val maxSuggestionCount: Int,
) : FillResponseBuilder {

    @SuppressLint("NewApi")
    @RequiresApi(Build.VERSION_CODES.O)
    override fun build(
        context: Context,
        configuration: AutofillConfiguration,
        imeSpec: InlinePresentationSpec?,
    ): FillResponse {
        val builder = FillResponse.Builder()

        val autofillIds = listOfNotNull(parsedStructure.usernameId, parsedStructure.passwordId)

        val title = context.getString(
            R.string.mozac_feature_autofill_popup_unlock_application,
            configuration.applicationName,
        )

        val authPresentation = RemoteViews(context.packageName, android.R.layout.simple_list_item_1).apply {
            setTextViewText(
                android.R.id.text1,
                title,
            )
        }

        val authIntent = Intent(context, configuration.unlockActivity)

        // Pass `ParsedStructure` as raw bytes to prevent the system throwing a ClassNotFoundException
        // when updating the PendingIntent and trying to create and remap `ParsedStructure`
        // from the parcelable extra because of an unknown ClassLoader.
        with(Parcel.obtain()) {
            parsedStructure.writeToParcel(this, 0)

            authIntent.putExtra(
                AbstractAutofillUnlockActivity.EXTRA_PARSED_STRUCTURE,
                this.marshall(),
            )

            recycle()
        }

        authIntent.putExtra(AbstractAutofillUnlockActivity.EXTRA_IME_SPEC, imeSpec)
        authIntent.putExtra(
            AbstractAutofillUnlockActivity.EXTRA_MAX_SUGGESTION_COUNT,
            maxSuggestionCount,
        )
        val authPendingIntent = PendingIntent.getActivity(
            context,
            configuration.activityRequestCode,
            authIntent,
            PendingIntent.FLAG_CANCEL_CURRENT or PendingIntent.FLAG_MUTABLE,
        )
        val intentSender: IntentSender = authPendingIntent.intentSender

        val icon: Icon = Icon.createWithResource(context, R.drawable.fingerprint_dialog_fp_icon)
        val authInlinePresentation = createInlinePresentation(authPendingIntent, imeSpec, title, icon)
        builder.setAuthentication(
            autofillIds.toTypedArray(),
            intentSender,
            authInlinePresentation,
            authPresentation,
        )

        return builder.build()
    }
}

@RequiresApi(Build.VERSION_CODES.O)
internal fun FillResponse.Builder.setAuthentication(
    ids: Array<AutofillId>,
    authentication: IntentSender,
    inlinePresentation: InlinePresentation? = null,
    presentation: RemoteViews,
): FillResponse.Builder {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && inlinePresentation != null) {
        this.setAuthentication(ids, authentication, presentation, inlinePresentation)
    } else {
        this.setAuthentication(ids, authentication, presentation)
    }
}
