/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

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
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * A customizable multiple login selection bar implementing [SelectablePromptView].
 */
class LoginSelectBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr), SelectablePromptView<Login> {

    var headerTextStyle: Int? = null

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.LoginSelectBar,
            defStyleAttr,
            0,
        ) {
            val textStyle =
                getResourceId(R.styleable.LoginSelectBar_mozacLoginSelectHeaderTextStyle, 0)
            if (textStyle > 0) {
                headerTextStyle = textStyle
            }
        }
    }

    override var listener: SelectablePromptView.Listener<Login>? = null

    override fun showPrompt(options: List<Login>) {
        if (loginPickerView == null) {
            loginPickerView =
                View.inflate(context, R.layout.mozac_feature_login_multiselect_view, this)
            bindViews()
        }

        listAdapter.submitList(options)
        loginPickerView?.isVisible = true
    }

    override fun hidePrompt() {
        this.isVisible = false
        loginsList?.isVisible = false
        listAdapter.submitList(mutableListOf())
        manageLoginsButton?.isVisible = false
        toggleSavedLoginsHeader(shouldExpand = false)
    }

    override fun asView(): View {
        return super.asView()
    }

    private var loginPickerView: View? = null
    private var loginsList: RecyclerView? = null
    private var manageLoginsButton: AppCompatTextView? = null
    private var savedLoginsHeader: AppCompatTextView? = null
    private var expandArrowHead: AppCompatImageView? = null

    private var listAdapter = BasicLoginAdapter {
        listener?.onOptionSelect(it)
    }

    private fun bindViews() {
        manageLoginsButton = findViewById<AppCompatTextView>(R.id.manage_logins).apply {
            setOnClickListener {
                listener?.onManageOptions()
            }
        }
        loginsList = findViewById(R.id.logins_list)
        savedLoginsHeader = findViewById<AppCompatTextView>(R.id.saved_logins_header).apply {
            headerTextStyle?.let {
                TextViewCompat.setTextAppearance(this, it)
                currentTextColor.let {
                    TextViewCompat.setCompoundDrawableTintList(this, ColorStateList.valueOf(it))
                }
            }
            setOnClickListener {
                toggleSavedLoginsHeader(shouldExpand = loginsList?.isVisible != true)
            }
        }
        expandArrowHead =
            findViewById<AppCompatImageView>(R.id.mozac_feature_login_multiselect_expand).apply {
                savedLoginsHeader?.currentTextColor?.let {
                    ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(it))
                }
            }
        loginsList?.apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false).also {
                val dividerItemDecoration = DividerItemDecoration(context, it.orientation)
                addItemDecoration(dividerItemDecoration)
            }
            adapter = listAdapter
        }
    }

    private fun toggleSavedLoginsHeader(shouldExpand: Boolean) {
        if (shouldExpand) {
            loginsList?.isVisible = true
            manageLoginsButton?.isVisible = true
            loginPickerView?.hideKeyboard()
            expandArrowHead?.rotation = ROTATE_180
            savedLoginsHeader?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_collapse_logins_content_description)
        } else {
            expandArrowHead?.rotation = 0F
            loginsList?.isVisible = false
            manageLoginsButton?.isVisible = false
            savedLoginsHeader?.contentDescription =
                context.getString(R.string.mozac_feature_prompts_expand_logins_content_description)
        }
    }

    companion object {
        private const val ROTATE_180 = 180F
    }
}
