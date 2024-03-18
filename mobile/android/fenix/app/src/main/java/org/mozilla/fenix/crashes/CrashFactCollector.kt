/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.feature.contextmenu.facts.ContextMenuFacts
import mozilla.components.feature.downloads.facts.DownloadsFacts
import mozilla.components.feature.prompts.facts.AddressAutofillDialogFacts
import mozilla.components.feature.prompts.facts.CreditCardAutofillDialogFacts
import mozilla.components.feature.prompts.facts.PromptFacts
import mozilla.components.feature.sitepermissions.SitePermissionsFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts

/**
 * Collects facts and record bread crumbs for the events.
 */
class CrashFactCollector(
    private val crashReporter: CrashReporting,
) {

    /**
     * Starts collecting facts.
     */
    fun start() {
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    fact.process()
                }
            },
        )
    }

    internal fun Fact.process(): Unit = when (component to item) {
        Component.FEATURE_CONTEXTMENU to ContextMenuFacts.Items.MENU,
        Component.FEATURE_DOWNLOADS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN,
        Component.FEATURE_DOWNLOADS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SAVE_PROMPT_SHOWN,
        Component.FEATURE_DOWNLOADS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED,
        Component.FEATURE_DOWNLOADS to AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_SHOWN,
        Component.FEATURE_DOWNLOADS to AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_DISMISSED,
        Component.FEATURE_DOWNLOADS to DownloadsFacts.Items.PROMPT,
        Component.FEATURE_SITEPERMISSIONS to SitePermissionsFacts.Items.PERMISSIONS,
        Component.FEATURE_PROMPTS to PromptFacts.Items.PROMPT,
        -> {
            crashReporter.recordCrashBreadcrumb(Breadcrumb("$component $action $value"))
        }
        else -> {
            // no-op
        }
    }
}
