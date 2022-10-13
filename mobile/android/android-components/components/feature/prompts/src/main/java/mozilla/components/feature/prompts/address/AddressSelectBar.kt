/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

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
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.facts.emitAddressAutofillExpandedFact
import mozilla.components.feature.prompts.facts.emitSuccessfulAddressAutofillSuccessFact
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * A customizable "Select addresses" bar implementing [SelectablePromptView].
 */
class AddressSelectBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr), SelectablePromptView<Address> {

    private var view: View? = null
    private var recyclerView: RecyclerView? = null
    private var headerView: AppCompatTextView? = null
    private var expanderView: AppCompatImageView? = null
    private var manageAddressesView: AppCompatTextView? = null
    private var headerTextStyle: Int? = null

    private val listAdapter = AddressAdapter { address ->
        listener?.apply {
            onOptionSelect(address)
            emitSuccessfulAddressAutofillSuccessFact()
        }
    }

    override var listener: SelectablePromptView.Listener<Address>? = null

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.AddressSelectBar,
            defStyleAttr,
            0,
        ) {
            val textStyle =
                getResourceId(
                    R.styleable.AddressSelectBar_mozacSelectAddressHeaderTextStyle,
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
        manageAddressesView?.isVisible = false

        listAdapter.submitList(null)

        toggleSelectAddressHeader(shouldExpand = false)
    }

    override fun showPrompt(options: List<Address>) {
        if (view == null) {
            view = View.inflate(context, LAYOUT_ID, this)
            bindViews()
        }

        listAdapter.submitList(options)
        view?.isVisible = true
    }

    private fun bindViews() {
        recyclerView = findViewById<RecyclerView>(R.id.address_list).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
            adapter = listAdapter
        }

        headerView = findViewById<AppCompatTextView>(R.id.select_address_header).apply {
            setOnClickListener {
                toggleSelectAddressHeader(shouldExpand = recyclerView?.isVisible != true)
            }

            headerTextStyle?.let { appearance ->
                TextViewCompat.setTextAppearance(this, appearance)
                currentTextColor.let { color ->
                    TextViewCompat.setCompoundDrawableTintList(this, ColorStateList.valueOf(color))
                }
            }
        }

        expanderView =
            findViewById<AppCompatImageView>(R.id.mozac_feature_address_expander).apply {
                headerView?.currentTextColor?.let {
                    ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(it))
                }
            }

        manageAddressesView = findViewById<AppCompatTextView>(R.id.manage_addresses).apply {
            setOnClickListener {
                listener?.onManageOptions()
            }
        }
    }

    /**
     * Toggles the visibility of the list of address items in the prompt.
     *
     * @param shouldExpand True if the list of addresses should be displayed, false otherwise.
     */
    private fun toggleSelectAddressHeader(shouldExpand: Boolean) {
        recyclerView?.isVisible = shouldExpand
        manageAddressesView?.isVisible = shouldExpand

        if (shouldExpand) {
            emitAddressAutofillExpandedFact()
            view?.hideKeyboard()
            expanderView?.rotation = ROTATE_180
            headerView?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_collapse_address_content_description)
        } else {
            expanderView?.rotation = 0F
            headerView?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_expand_address_content_description)
        }
    }

    companion object {
        val LAYOUT_ID = R.layout.mozac_feature_prompts_address_select_prompt

        private const val ROTATE_180 = 180F
    }
}
