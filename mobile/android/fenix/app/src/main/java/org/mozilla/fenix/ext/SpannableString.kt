/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import android.text.Spannable
import android.text.SpannableString
import android.text.style.AbsoluteSizeSpan
import android.text.style.ForegroundColorSpan
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToPx

fun SpannableString.setTextSize(context: Context, textSize: Int) =
    this.setSpan(
        AbsoluteSizeSpan(textSize.dpToPx(context.resources.displayMetrics)),
        0,
        this.length,
        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
    )

fun SpannableString.setTextColor(context: Context, colorResId: Int) =
    this.setSpan(
        ForegroundColorSpan(
            context.getColorFromAttr(colorResId)
        ),
        0,
        this.length,
        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
    )
