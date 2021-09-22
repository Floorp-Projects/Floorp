/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.tips

import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.textview.MaterialTextView
import org.mozilla.focus.R

class TipsAdapter : ListAdapter<Tip, TipsAdapter.TipViewHolder>(TipsDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TipViewHolder {
        val layoutInflater = LayoutInflater.from(parent.context)
        val rootView =
            layoutInflater.inflate(R.layout.tip_carousel_item, parent, false) as MaterialTextView
        return TipViewHolder(rootView)
    }

    override fun onBindViewHolder(holder: TipViewHolder, position: Int) =
        holder.bind(getItem(position))

    class TipViewHolder(private val tipView: MaterialTextView) :
        RecyclerView.ViewHolder(tipView) {

        fun bind(tip: Tip) {
            val tipText = if (tip.appName != null) {
                String.format(tipView.context.getString(tip.id), tip.appName)
            } else {
                tipView.context.getString(tip.id)
            }
            if (tip.deepLink == null) {
                tipView.text = tipText
            } else {
                tipView.movementMethod = LinkMovementMethod()
                tipView.setText(tipText, TextView.BufferType.SPANNABLE)
                tipView.setOnClickListener { tip.deepLink.invoke() }

                val spanStartIndex = if (tipText.contains("\n")) tipText.indexOf("\n") + 2 else 0
                val spannedText = SpannableString(tipText).apply {
                    val colorSpan = ForegroundColorSpan(
                        ContextCompat.getColor(
                            tipView.context,
                            R.color.tip_deeplink_color
                        )
                    )
                    setSpan(colorSpan, spanStartIndex, tipText.length, 0)
                }

                tipView.text = spannedText
            }
        }
    }

    class TipsDiffCallback : DiffUtil.ItemCallback<Tip>() {
        override fun areItemsTheSame(oldItem: Tip, newItem: Tip) = oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: Tip, newItem: Tip) = oldItem.id == newItem.id
    }
}
