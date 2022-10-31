/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.widget.AppCompatImageView
import androidx.appcompat.widget.AppCompatTextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import androidx.core.view.isVisible
import androidx.core.widget.ImageViewCompat
import androidx.core.widget.TextViewCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.facts.emitCreditCardAutofillExpandedFact
import mozilla.components.feature.prompts.facts.emitSuccessfulCreditCardAutofillSuccessFact
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * A customizable "Select credit card" bar implementing [SelectablePromptView].
 */
class CreditCardSelectBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr), SelectablePromptView<CreditCardEntry> {

    private var view: View? = null
    private var recyclerView: RecyclerView? = null
    private var headerView: AppCompatTextView? = null
    private var expanderView: AppCompatImageView? = null
    private var manageCreditCardsButtonView: AppCompatTextView? = null
    private var headerTextStyle: Int? = null

    private val listAdapter = CreditCardsAdapter { creditCard ->
        listener?.apply {
            onOptionSelect(creditCard)
            emitSuccessfulCreditCardAutofillSuccessFact()
        }
    }

    override var listener: SelectablePromptView.Listener<CreditCardEntry>? = null

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.CreditCardSelectBar,
            defStyleAttr,
            0,
        ) {
            val textStyle =
                getResourceId(
                    R.styleable.CreditCardSelectBar_mozacSelectCreditCardHeaderTextStyle,
                    0,
                )

            if (textStyle > 0) {
                headerTextStyle = textStyle
            }
        }
    }

    override fun hidePrompt() {
        this.isVisible = false
        recyclerView?.isVisible = false
        manageCreditCardsButtonView?.isVisible = false

        listAdapter.submitList(null)

        toggleSelectCreditCardHeader(shouldExpand = false)
    }

    override fun showPrompt(options: List<CreditCardEntry>) {
        if (view == null) {
            view = View.inflate(context, LAYOUT_ID, this)
            bindViews()
        }

        listAdapter.submitList(options)
        view?.isVisible = true
    }

    private fun bindViews() {
        recyclerView = findViewById<RecyclerView>(R.id.credit_cards_list).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
            adapter = listAdapter
        }

        headerView = findViewById<AppCompatTextView>(R.id.select_credit_card_header).apply {
            setOnClickListener {
                toggleSelectCreditCardHeader(shouldExpand = recyclerView?.isVisible != true)
            }

            headerTextStyle?.let {
                TextViewCompat.setTextAppearance(this, it)
                currentTextColor.let {
                    TextViewCompat.setCompoundDrawableTintList(this, ColorStateList.valueOf(it))
                }
            }
        }

        expanderView =
            findViewById<AppCompatImageView>(R.id.mozac_feature_credit_cards_expander).apply {
                headerView?.currentTextColor?.let {
                    ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(it))
                }
            }

        manageCreditCardsButtonView =
            findViewById<AppCompatTextView>(R.id.manage_credit_cards).apply {
                setOnClickListener {
                    listener?.onManageOptions()
                }
            }
    }

    /**
     * Toggles the visibility of the list of credit cards in the prompt.
     *
     * @param shouldExpand True if the list of credit cards should be displayed, false otherwise.
     */
    private fun toggleSelectCreditCardHeader(shouldExpand: Boolean) {
        recyclerView?.isVisible = shouldExpand
        manageCreditCardsButtonView?.isVisible = shouldExpand

        if (shouldExpand) {
            view?.hideKeyboard()
            expanderView?.rotation = ROTATE_180
            headerView?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_collapse_credit_cards_content_description)
            emitCreditCardAutofillExpandedFact()
        } else {
            expanderView?.rotation = 0F
            headerView?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_expand_credit_cards_content_description)
        }
    }

    companion object {
        val LAYOUT_ID = R.layout.mozac_feature_prompts_credit_card_select_prompt

        private const val ROTATE_180 = 180F
    }
}
