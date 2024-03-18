/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class CreditCardAutofillDialogFactsTest {

    @Test
    fun `Emits facts for autofill form detected events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuccessfulCreditCardAutofillFormDetectedFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_FORM_DETECTED, item)
            }
        }
    }

    @Test
    fun `Emits facts for autofill success events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuccessfulCreditCardAutofillSuccessFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS, item)
            }
        }
    }

    @Test
    fun `Emits facts for autofill shown events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitCreditCardAutofillShownFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN, item)
            }
        }
    }

    @Test
    fun `Emits facts for autofill expanded events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitCreditCardAutofillExpandedFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED, item)
            }
        }
    }

    @Test
    fun `Emits facts for autofill dismissed events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitCreditCardAutofillDismissedFact()

            assertEquals(1, facts.size)

            facts[0].apply {
                assertEquals(Component.FEATURE_PROMPTS, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED, item)
            }
        }
    }

    @Test
    fun `Emits facts for autofill confirm and create events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitCreditCardAutofillCreatedFact()

            assertEquals(1, facts.size)

            val fact = facts.single()
            assertEquals(Component.FEATURE_PROMPTS, fact.component)
            assertEquals(Action.CONFIRM, fact.action)
            assertEquals(
                CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_CREATED,
                fact.item,
            )
        }
    }

    @Test
    fun `Emits facts for autofill confirm and update events`() {
        CollectionProcessor.withFactCollection { facts ->

            emitCreditCardAutofillUpdatedFact()

            assertEquals(1, facts.size)

            val fact = facts.single()
            assertEquals(Component.FEATURE_PROMPTS, fact.component)
            assertEquals(Action.CONFIRM, fact.action)
            assertEquals(
                CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_UPDATED,
                fact.item,
            )
        }
    }

    @Test
    fun `Emits facts for autofill save prompt shown event`() {
        CollectionProcessor.withFactCollection { facts ->
            emitCreditCardSaveShownFact()

            assertEquals(1, facts.size)

            val fact = facts.single()
            assertEquals(Component.FEATURE_PROMPTS, fact.component)
            assertEquals(Action.DISPLAY, fact.action)
            assertEquals(
                CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SAVE_PROMPT_SHOWN,
                fact.item,
            )
        }
    }
}
