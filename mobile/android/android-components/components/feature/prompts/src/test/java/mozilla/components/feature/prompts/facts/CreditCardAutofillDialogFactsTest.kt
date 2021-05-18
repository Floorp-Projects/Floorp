/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.facts

import mozilla.components.feature.prompts.facts.CreditCardAutofillDialogFacts
import mozilla.components.feature.prompts.facts.emitSuccessfulCreditCardAutofillFact
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class CreditCardAutofillDialogFactsTest {
    @Test
    fun `Emits facts for autofill events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuccessfulCreditCardAutofillFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS, item)
            }
        }
    }
}
