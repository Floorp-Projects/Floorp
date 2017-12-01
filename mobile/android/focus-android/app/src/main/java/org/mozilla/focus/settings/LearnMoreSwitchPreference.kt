package org.mozilla.focus.settings

import android.content.Context
import android.graphics.Paint
import android.preference.SwitchPreference
import android.support.v4.content.ContextCompat
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.activity.InfoActivity

abstract class LearnMoreSwitchPreference(context: Context?, attrs: AttributeSet?) : SwitchPreference(context, attrs) {
    init {
        layoutResource = R.layout.preference_switch_learn_more
    }

    override fun onBindView(view: View?) {
        super.onBindView(view)

        if (view == null) {
            return
        }

        getDescription()?.let {
            val summaryView = view.findViewById<TextView>(android.R.id.summary)
            summaryView.text = it
            summaryView.visibility = View.VISIBLE
        }

        val learnMoreLink = view.findViewById<TextView>(R.id.link)
        learnMoreLink.paintFlags = learnMoreLink.paintFlags or Paint.UNDERLINE_TEXT_FLAG
        learnMoreLink.setTextColor(ContextCompat.getColor(view.context, R.color.colorAction))
        learnMoreLink.setOnClickListener {
            // This is a hardcoded link: if we ever end up needing more of these links, we should
            // move the link into an xml parameter, but there's no advantage to making it configurable now.
            val intent = InfoActivity.getIntentFor(context, getLearnMoreUrl(), title.toString())
            context.startActivity(intent)
        }

        val backgroundDrawableArray = view.context.obtainStyledAttributes(intArrayOf(R.attr.selectableItemBackground))
        val backgroundDrawable = backgroundDrawableArray.getDrawable(0)
        backgroundDrawableArray.recycle()
        learnMoreLink.background = backgroundDrawable
    }

    open fun getDescription(): String? = null

    abstract fun getLearnMoreUrl(): String
}
