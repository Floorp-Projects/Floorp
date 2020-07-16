/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.res.Resources
import android.text.Spanned
import android.view.LayoutInflater
import android.view.View
import androidx.core.text.HtmlCompat
import androidx.core.text.HtmlCompat.TO_HTML_PARAGRAPH_LINES_CONSECUTIVE
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.android.synthetic.main.onboarding_whats_new.view.*
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingWhatsNewViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: OnboardingInteractor

    @Before
    fun setup() {
        mockkStatic("mozilla.components.support.ktx.android.content.res.ThemeKt")
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingWhatsNewViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)

        every {
            any<Resources.Theme>().resolveAttribute(R.attr.onboardingSelected)
        } returns R.color.onboarding_illustration_selected_normal_theme
    }

    @After
    fun teardown() {
        unmockkStatic("mozilla.components.support.ktx.android.content.res.ThemeKt")
    }

    @Test
    fun `sets and styles strings`() {
        OnboardingWhatsNewViewHolder(view, interactor)

        assertEquals(
            "Have questions about the redesigned Firefox Preview? Want to know what’s changed?",
            view.description_text.text
        )

        val getAnswersHtml = HtmlCompat.toHtml(view.get_answers.text as Spanned, TO_HTML_PARAGRAPH_LINES_CONSECUTIVE)
        assertTrue(getAnswersHtml, "<u>Get answers here</u>" in getAnswersHtml)
    }

    @Test
    fun `call interactor on click`() {
        OnboardingWhatsNewViewHolder(view, interactor)

        view.get_answers.performClick()
        verify { interactor.onWhatsNewGetAnswersClicked() }
    }
}
