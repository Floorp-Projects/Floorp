/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.mozonline

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.text.SpannableString
import android.text.Spanned
import android.text.method.LinkMovementMethod
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricServiceType
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import kotlin.system.exitProcess

fun showPrivacyPopWindow(context: Context, activity: Activity) {
    val content = context.getString(R.string.privacy_notice_content)

    // Use hyperlinks to display details about privacy
    val messageClickable1 = context.getString(R.string.privacy_notice_clickable1)
    val messageClickable2 = context.getString(R.string.privacy_notice_clickable2)
    val messageClickable3 = context.getString(R.string.privacy_notice_clickable3)
    val messageSpannable = SpannableString(content)

    val clickableSpan1 = PrivacyContentSpan(Position.POS1, context)
    val clickableSpan2 = PrivacyContentSpan(Position.POS2, context)
    val clickableSpan3 = PrivacyContentSpan(Position.POS3, context)

    messageSpannable.setSpan(
        clickableSpan1,
        content.indexOf(messageClickable1),
        content.indexOf(messageClickable1) + messageClickable1.length,
        Spanned.SPAN_INCLUSIVE_INCLUSIVE,
    )
    messageSpannable.setSpan(
        clickableSpan2,
        content.indexOf(messageClickable2),
        content.indexOf(messageClickable2) + messageClickable2.length,
        Spanned.SPAN_INCLUSIVE_INCLUSIVE,
    )
    messageSpannable.setSpan(
        clickableSpan3,
        content.indexOf(messageClickable3),
        content.indexOf(messageClickable3) + messageClickable3.length,
        Spanned.SPAN_INCLUSIVE_INCLUSIVE,
    )

    // Users can only use fenix after they agree with the privacy notice
    val builder = AlertDialog.Builder(activity)
        .setPositiveButton(
            context.getString(R.string.privacy_notice_positive_button),
        ) { _, _ ->
            context.settings().shouldShowPrivacyPopWindow = false
            context.settings().isMarketingTelemetryEnabled = true
            context.components.analytics.metrics.start(MetricServiceType.Marketing)
            // Now that the privacy notice is accepted, application initialization can continue.
            context.application.initialize()
            activity.startActivity(Intent(activity, HomeActivity::class.java))
            activity.finish()
        }
        .setNeutralButton(
            context.getString(R.string.privacy_notice_neutral_button_2),
            { _, _ -> exitProcess(0) },
        )
        .setTitle(context.getString(R.string.privacy_notice_title))
        .setMessage(messageSpannable)
        .setCancelable(false)
    val alertDialog: AlertDialog = builder.create().withCenterAlignedButtons()
    alertDialog.show()
    alertDialog.findViewById<TextView>(android.R.id.message)?.movementMethod = LinkMovementMethod.getInstance()
}
