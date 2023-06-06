/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.util.AttributeSet
import android.widget.ImageView
import androidx.appcompat.widget.AppCompatRadioButton
import androidx.core.content.edit
import androidx.core.content.withStyledAttributes
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.setTextSize
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.view.GroupableRadioButton
import org.mozilla.fenix.utils.view.uncheckAll

class OnboardingRadioButton(
    context: Context,
    attrs: AttributeSet,
) : AppCompatRadioButton(context, attrs), GroupableRadioButton {
    private val radioGroups = mutableListOf<GroupableRadioButton>()
    private var illustration: ImageView? = null
    private var clickListener: (() -> Unit)? = null
    var key: Int = 0
    var title: Int = 0
    var description: Int = 0

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.OnboardingRadioButton,
            0,
            0,
        ) {
            key = getResourceId(R.styleable.OnboardingRadioButton_onboardingKey, 0)
            title = getResourceId(R.styleable.OnboardingRadioButton_onboardingKeyTitle, 0)
            description =
                getResourceId(R.styleable.OnboardingRadioButton_onboardingKeyDescription, 0)
        }
    }

    override fun addToRadioGroup(radioButton: GroupableRadioButton) {
        radioGroups.add(radioButton)
    }

    fun addIllustration(illustration: ImageView) {
        this.illustration = illustration
    }

    fun onClickListener(listener: () -> Unit) {
        clickListener = listener
    }

    init {
        setOnClickListener {
            updateRadioValue(true)
            toggleRadioGroups()
            clickListener?.invoke()
        }
        if (title != 0) {
            setRadioButtonText(context)
        }
    }

    private fun setRadioButtonText(context: Context) {
        val builder = SpannableStringBuilder()

        val spannableTitle = SpannableString(resources.getString(title))
        spannableTitle.setTextSize(context, TITLE_TEXT_SIZE)
        spannableTitle.setTextColor(context, R.attr.textPrimary)

        builder.append(spannableTitle)

        if (description != 0) {
            val spannableDescription = SpannableString(resources.getString(description))
            spannableDescription.setTextSize(context, DESCRIPTION_TEXT_SIZE)
            spannableDescription.setTextColor(context, R.attr.textSecondary)
            builder.append("\n")
            builder.append(spannableDescription)
        }
        this.text = builder
    }

    override fun updateRadioValue(isChecked: Boolean) {
        this.isChecked = isChecked
        illustration?.let {
            it.isSelected = isChecked
        }
        CoroutineScope(Dispatchers.IO).launch {
            context.settings().preferences.edit {
                putBoolean(context.getString(key), isChecked)
            }
        }
    }

    private fun toggleRadioGroups() {
        if (isChecked) {
            radioGroups.uncheckAll()
        }
    }

    companion object {
        private const val TITLE_TEXT_SIZE = 16
        private const val DESCRIPTION_TEXT_SIZE = 14
    }
}
