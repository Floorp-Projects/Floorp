/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * [Fact]s emitted by the `feature-autofill` component.
 */
class AutofillFacts {
    /**
     * Items the `feature-autofill` component emits [Fact]s for.
     */
    object Items {
        const val AUTOFILL_REQUEST = "autofill_request"
        const val AUTOFILL_CONFIRMATION = "autofill_confirmation"
        const val AUTOFILL_SEARCH = "autofill_search"
        const val AUTOFILL_LOCK = "autofill_lock"
    }

    /**
     * Metadata keys used by some [Fact]s emitted by the `feature-autofill` component.
     */
    object Metadata {
        const val HAS_MATCHING_LOGINS = "has_matching_logins"
        const val NEEDS_CONFIRMATION = "needs_confirmation"
    }
}

internal fun emitAutofillRequestFact(
    hasLogins: Boolean,
    needsConfirmation: Boolean? = null,
) {
    Fact(
        Component.FEATURE_AUTOFILL,
        Action.SYSTEM,
        AutofillFacts.Items.AUTOFILL_REQUEST,
        metadata = requestMetadata(hasLogins, needsConfirmation),
    ).collect()
}

internal fun emitAutofillConfirmationFact(
    confirmed: Boolean,
) {
    Fact(
        Component.FEATURE_AUTOFILL,
        if (confirmed) { Action.CONFIRM } else { Action.CANCEL },
        AutofillFacts.Items.AUTOFILL_CONFIRMATION,
    ).collect()
}

internal fun emitAutofillSearchDisplayedFact() {
    Fact(
        Component.FEATURE_AUTOFILL,
        Action.DISPLAY,
        AutofillFacts.Items.AUTOFILL_SEARCH,
    ).collect()
}

internal fun emitAutofillSearchSelectedFact() {
    Fact(
        Component.FEATURE_AUTOFILL,
        Action.SELECT,
        AutofillFacts.Items.AUTOFILL_SEARCH,
    ).collect()
}

internal fun emitAutofillLock(
    unlocked: Boolean,
) {
    Fact(
        Component.FEATURE_AUTOFILL,
        if (unlocked) { Action.CONFIRM } else { Action.CANCEL },
        AutofillFacts.Items.AUTOFILL_LOCK,
    ).collect()
}

private fun requestMetadata(
    hasLogins: Boolean,
    needsConfirmation: Boolean? = null,
): Map<String, Any> {
    val metadata = mutableMapOf<String, Any>(
        AutofillFacts.Metadata.HAS_MATCHING_LOGINS to hasLogins,
    )

    needsConfirmation?.let {
        metadata[AutofillFacts.Metadata.NEEDS_CONFIRMATION] = needsConfirmation
    }

    return metadata
}
