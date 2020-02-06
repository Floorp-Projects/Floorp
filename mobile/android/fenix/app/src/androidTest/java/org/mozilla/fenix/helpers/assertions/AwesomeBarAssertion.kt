package org.mozilla.fenix.helpers.assertions

import android.view.View
import androidx.test.espresso.ViewAssertion
import mozilla.components.browser.awesomebar.BrowserAwesomeBar

class AwesomeBarAssertion {
    companion object {
        fun suggestionsAreGreaterThan(minimumSuggestions: Int): ViewAssertion {
            return ViewAssertion { view, noViewFoundException ->
                if (noViewFoundException != null) throw noViewFoundException

                val suggestionsCount = getSuggestionCountFromView(view)

                if (suggestionsCount <= minimumSuggestions)
                    throw AssertionError("The suggestion count is less than or equal to the minimum suggestions")
            }
        }

        fun suggestionsAreEqualTo(expectedItemCount: Int): ViewAssertion {
            return ViewAssertion { view, noViewFoundException ->
                if (noViewFoundException != null) throw noViewFoundException

                val suggestionsCount = getSuggestionCountFromView(view)

                if (suggestionsCount != expectedItemCount)
                    throw AssertionError("The expected item count is $expectedItemCount, and the suggestions count within the AwesomeBar is $suggestionsCount")
            }
        }

        private fun getSuggestionCountFromView(view: View): Int {
            return (view as BrowserAwesomeBar).adapter?.itemCount
                ?: throw AssertionError("This view is not of type BrowserAwesomeBar")
        }
    }
}
