/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.widget.AppCompatTextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import androidx.core.view.isVisible
import androidx.core.widget.TextViewCompat
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.PasswordPromptView

/**
 * A prompt bar implementing [PasswordPromptView] to display the strong generated password.
 */
class SuggestStrongPasswordBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr), PasswordPromptView {

    private var headerTextStyle: Int? = null
    private var suggestStrongPasswordView: View? = null
    private var suggestStrongPasswordHeader: AppCompatTextView? = null
    private var useStrongPasswordTitle: AppCompatTextView? = null

    override var listener: PasswordPromptView.Listener? = null

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

    override fun showPrompt(
        generatedPassword: String,
        url: String,
        onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
    ) {
        if (suggestStrongPasswordView == null) {
            suggestStrongPasswordView =
                View.inflate(context, R.layout.mozac_feature_suggest_strong_password_view, this)
            bindViews(generatedPassword, url, onSaveLoginWithStrongPassword)
        }
        suggestStrongPasswordView?.isVisible = true
        useStrongPasswordTitle?.isVisible = false
    }

    override fun hidePrompt() {
        isVisible = false
    }

    private fun bindViews(
        strongPassword: String,
        url: String,
        onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
    ) {
        suggestStrongPasswordHeader =
            findViewById<AppCompatTextView>(R.id.suggest_strong_password_header).apply {
                headerTextStyle?.let {
                    TextViewCompat.setTextAppearance(this, it)
                    currentTextColor.let { textColor ->
                        TextViewCompat.setCompoundDrawableTintList(
                            this,
                            ColorStateList.valueOf(textColor),
                        )
                    }
                }
                setOnClickListener {
                    useStrongPasswordTitle?.let {
                        it.visibility = if (it.isVisible) {
                            GONE
                        } else {
                            VISIBLE
                        }
                    }
                }
            }

        useStrongPasswordTitle = findViewById<AppCompatTextView>(R.id.use_strong_password).apply {
            text = context.getString(
                R.string.mozac_feature_prompts_suggest_strong_password_message,
                strongPassword,
            )
            visibility = GONE
            setOnClickListener {
                listener?.onUseGeneratedPassword(
                    generatedPassword = strongPassword,
                    url = url,
                    onSaveLoginWithStrongPassword = onSaveLoginWithStrongPassword,
                )
            }
        }
    }
}
