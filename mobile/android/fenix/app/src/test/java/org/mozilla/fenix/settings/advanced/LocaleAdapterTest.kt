package org.mozilla.fenix.settings.advanced

import android.content.Context
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.locale.LocaleManager
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.Locale

class LocaleAdapterTest {

    private val selectedLocale = Locale("en", "UK")
    private val view: View = mockk(relaxed = true)
    private val context: Context = mockk(relaxed = true)

    private val localeViewHolder: BaseLocaleViewHolder =
        object : BaseLocaleViewHolder(view, selectedLocale) {

            override fun bind(locale: Locale) {
                // not required
            }
        }

    @Before
    fun setup() {
        every { view.context } returns context
    }

    @Test
    fun `verify selected locale checker returns true`() {
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.isDefaultLocaleSelected(context) } returns false

        assertTrue(localeViewHolder.isCurrentLocaleSelected(selectedLocale, isDefault = false))
    }

    @Test
    fun `verify default locale checker returns true`() {
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        every { LocaleManager.isDefaultLocaleSelected(context) } returns true

        assertTrue(localeViewHolder.isCurrentLocaleSelected(selectedLocale, isDefault = true))
    }
}
