/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tips

import android.view.View
import androidx.core.view.isVisible
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.databinding.ButtonTipItemBinding
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.view.ViewHolder

class ButtonTipViewHolder(
    private val view: View,
    private val interactor: SessionControlInteractor,
    private val metrics: MetricController = view.context.components.analytics.metrics,
    private val settings: Settings = view.context.components.settings
) : ViewHolder(view) {

    var tip: Tip? = null

    fun bind(tip: Tip) {
        val binding = ButtonTipItemBinding.bind(view)
        require(tip.type is TipType.Button)

        this.tip = tip

        metrics.track(Event.TipDisplayed(tip.identifier))

        with(binding) {
            tipHeaderText.text = tip.title
            tip.titleDrawable?.let {
                tipHeaderText.setCompoundDrawablesWithIntrinsicBounds(it, null, null, null)
            }
            tipDescriptionText.text = tip.description
            tipButton.text = tip.type.text

            tipLearnMore.isVisible = tip.learnMoreURL != null
            if (tip.learnMoreURL != null) {
                tipLearnMore.addUnderline()

                tipLearnMore.setOnClickListener {
                    (itemView.context as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = tip.learnMoreURL,
                        newTab = true,
                        from = BrowserDirection.FromHome
                    )
                }
            }

            tipButton.setOnClickListener {
                tip.type.action.invoke()
                metrics.track(Event.TipPressed(tip.identifier))
            }

            tipClose.setOnClickListener {
                metrics.track(Event.TipClosed(tip.identifier))

                settings.preferences
                    .edit()
                    .putBoolean(tip.identifier, false)
                    .apply()

                interactor.onCloseTip(tip)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.button_tip_item
    }
}
