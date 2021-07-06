/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ImageSpan
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.getDrawableWithTint
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.OnboardingPrivateBrowsingBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.setBounds
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

class OnboardingPrivateBrowsingViewHolder(
    view: View,
    private val interactor: OnboardingInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        val binding = OnboardingPrivateBrowsingBinding.bind(view)
        binding.headerText.setOnboardingIcon(R.drawable.ic_onboarding_private_browsing)

        // Display a private browsing icon as a character inside the description text.
        val inlineIcon = PrivateBrowsingImageSpan(
            view.context,
            R.drawable.ic_private_browsing,
            tint = view.context.getColorFromAttr(R.attr.primaryText),
            size = binding.descriptionTextOnce.lineHeight
        )

        val text = SpannableString(view.context.getString(R.string.onboarding_private_browsing_description1)).apply {
            val spanStartIndex = indexOf(IMAGE_PLACEHOLDER)
            setSpan(
                    inlineIcon,
                spanStartIndex,
                spanStartIndex + IMAGE_PLACEHOLDER.length,
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        binding.descriptionTextOnce.text = text
        binding.descriptionTextOnce.contentDescription = String.format(text.toString(), binding.headerText.text)
        binding.openSettingsButton.setOnClickListener {
            it.context.components.analytics.metrics.track(Event.OnboardingPrivateBrowsing)
            interactor.onOpenSettingsClicked()
        }
    }

    class PrivateBrowsingImageSpan(
        context: Context,
        @DrawableRes drawableId: Int,
        @ColorInt tint: Int,
        size: Int
    ) : ImageSpan(
        context.getDrawableWithTint(drawableId, tint)!!.apply { setBounds(size) }
    ) {
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

    companion object {
        const val IMAGE_PLACEHOLDER = "%s"
        const val LAYOUT_ID = R.layout.onboarding_private_browsing
    }
}
