package org.mozilla.fenix.home.tips

import android.text.SpannableString
import android.text.style.UnderlineSpan
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.button_tip_item.view.*
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

class ButtonTipViewHolder(
    val view: View,
    val interactor: SessionControlInteractor
) : RecyclerView.ViewHolder(view) {
    var tip: Tip? = null

    fun bind(tip: Tip) {
        require(tip.type is TipType.Button)

        this.tip = tip

        view.apply {
            context.components.analytics.metrics.track(Event.TipDisplayed(tip.identifier))

            tip_header_text.text = tip.title
            tip_description_text.text = tip.description
            tip_button.text = tip.type.text

            if (tip.learnMoreURL == null) {
                tip_learn_more.visibility = View.GONE
            } else {
                val learnMoreText = context.getString(R.string.search_suggestions_onboarding_learn_more_link)
                val textWithLink = SpannableString(learnMoreText).apply {
                    setSpan(UnderlineSpan(), 0, learnMoreText.length, 0)
                }

                tip_learn_more.text = textWithLink

                tip_learn_more.setOnClickListener {
                    (context as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = tip.learnMoreURL,
                        newTab = true,
                        from = BrowserDirection.FromHome
                    )
                }
            }

            tip_button.setOnClickListener {
                tip.type.action.invoke()
                context.components.analytics.metrics.track(
                    Event.TipPressed(tip.identifier)
                )
            }

            tip_close.setOnClickListener {
                context.components.analytics.metrics.track(Event.TipClosed(tip.identifier))

                context.settings().preferences
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
