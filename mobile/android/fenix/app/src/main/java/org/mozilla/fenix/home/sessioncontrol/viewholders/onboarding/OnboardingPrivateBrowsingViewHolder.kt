/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.graphics.Canvas
import android.graphics.Paint
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ImageSpan
import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_private_browsing.view.*
import org.mozilla.fenix.R

class OnboardingPrivateBrowsingViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val iconDrawable = AppCompatResources.getDrawable(view.context, R.drawable.ic_private_browsing)!!
        iconDrawable.setBounds(0, 0, view.description_text.lineHeight, view.description_text.lineHeight)

        val icon = object : ImageSpan(iconDrawable) {
            override fun draw(
                canvas: Canvas,
                text: CharSequence?,
                start: Int,
                end: Int,
                x: Float,
                top: Int,
                y: Int,
                bottom: Int,
                paint: Paint
            ) {
                canvas.save()
                val fmPaint = paint.fontMetricsInt
                val fontHeight = fmPaint.descent - fmPaint.ascent
                val centerY = y + fmPaint.descent - fontHeight / 2
                val transY = (centerY - (drawable.bounds.bottom - drawable.bounds.top) / 2).toFloat()
                canvas.translate(x, transY)
                drawable.draw(canvas)
                canvas.restore()
            }
        }

        val text = SpannableString(view.context.getString(R.string.onboarding_private_browsing_description))
        val spanStartIndex = text.indexOf(IMAGE_PLACEHOLDER)

        text.setSpan(
            icon,
            spanStartIndex,
            spanStartIndex + IMAGE_PLACEHOLDER.length,
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
        )

        view.description_text.text = text
    }

    companion object {
        const val IMAGE_PLACEHOLDER = "%s"
        const val LAYOUT_ID = R.layout.onboarding_private_browsing
    }
}
