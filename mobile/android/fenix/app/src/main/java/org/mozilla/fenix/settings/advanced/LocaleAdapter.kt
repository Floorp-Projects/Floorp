/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.locale_settings_item.view.locale_selected_icon
import kotlinx.android.synthetic.main.locale_settings_item.view.locale_subtitle_text
import kotlinx.android.synthetic.main.locale_settings_item.view.locale_title_text
import org.mozilla.fenix.R
import java.util.Locale

class LocaleAdapter(private val interactor: LocaleSettingsViewInteractor) :
    RecyclerView.Adapter<BaseLocaleViewHolder>() {

    private var localeList: List<Locale> = listOf()
    private var selectedLocale: Locale = Locale.getDefault()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BaseLocaleViewHolder {
        val view =
            LayoutInflater.from(parent.context)
                .inflate(R.layout.locale_settings_item, parent, false)

        return when (viewType) {
            ItemType.DEFAULT.ordinal -> SystemLocaleViewHolder(
                view,
                interactor,
                selectedLocale
            )
            ItemType.LOCALE.ordinal -> LocaleViewHolder(
                view,
                interactor,
                selectedLocale
            )
            else -> throw IllegalStateException("ViewType $viewType does not match to a ViewHolder")
        }
    }

    override fun getItemCount(): Int {
        return localeList.size
    }

    override fun onBindViewHolder(holder: BaseLocaleViewHolder, position: Int) {
        holder.bind(localeList[position])
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            0 -> ItemType.DEFAULT
            else -> ItemType.LOCALE
        }.ordinal
    }

    fun updateData(localeList: List<Locale>, selectedLocale: Locale) {
        val diffUtil = DiffUtil.calculateDiff(
            LocaleDiffUtil(
                this.localeList,
                localeList,
                this.selectedLocale,
                selectedLocale
            )
        )
        this.localeList = localeList
        this.selectedLocale = selectedLocale

        diffUtil.dispatchUpdatesTo(this)
    }

    inner class LocaleDiffUtil(
        private val old: List<Locale>,
        private val new: List<Locale>,
        private val oldSelectedLocale: Locale,
        private val newSelectedLocale: Locale
    ) : DiffUtil.Callback() {

        override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
            val selectionChanged =
                old[oldItemPosition] == oldSelectedLocale && oldSelectedLocale != newSelectedLocale
            return old[oldItemPosition] == new[newItemPosition] && !selectionChanged
        }

        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            old[oldItemPosition].toLanguageTag() == new[newItemPosition].toLanguageTag()

        override fun getOldListSize(): Int = old.size
        override fun getNewListSize(): Int = new.size
    }

    enum class ItemType {
        DEFAULT, LOCALE;
    }
}

class LocaleViewHolder(
    view: View,
    private val interactor: LocaleSettingsViewInteractor,
    private val selectedLocale: Locale
) : BaseLocaleViewHolder(view) {
    private val icon = view.locale_selected_icon
    private val title = view.locale_title_text
    private val subtitle = view.locale_subtitle_text

    override fun bind(locale: Locale) {
        // capitalisation is done using the rules of the appropriate locale (endonym and exonym)
        title.text = locale.getDisplayName(locale).capitalize(locale)
        subtitle.text = locale.displayName.capitalize(Locale.getDefault())
        icon.isVisible = locale === selectedLocale

        itemView.setOnClickListener {
            interactor.onLocaleSelected(locale)
        }
    }
}

class SystemLocaleViewHolder(
    view: View,
    private val interactor: LocaleSettingsViewInteractor,
    private val selectedLocale: Locale
) : BaseLocaleViewHolder(view) {
    private val icon = view.locale_selected_icon
    private val title = view.locale_title_text
    private val subtitle = view.locale_subtitle_text

    override fun bind(locale: Locale) {
        title.text = itemView.context.getString(R.string.default_locale_text)
        subtitle.visibility = View.GONE
        icon.isVisible = locale === selectedLocale

        itemView.setOnClickListener {
            interactor.onDefaultLocaleSelected()
        }
    }
}

abstract class BaseLocaleViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    abstract fun bind(locale: Locale)
}

/**
 * Similar to Kotlin's capitalize with locale parameter, but that method is currently experimental
 */
private fun String.capitalize(locale: Locale): String {
    return substring(0, 1).toUpperCase(locale) + substring(1)
}
