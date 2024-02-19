# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## OS Prompt Dialog

# The macos string is preceded by the operating system (macOS) with "Firefox is trying to ",
# and has a period added to its end. Make sure to test in your locale.
autofill-use-payment-method-os-prompt-macos = use stored payment method information
autofill-use-payment-method-os-prompt-windows = { -brand-short-name } is trying to use stored payment method information. Confirm access to this Windows account below.
autofill-use-payment-method-os-prompt-other = { -brand-short-name } is trying to use stored payment method information.


# In macOS, this string is preceded by the operating system with "Firefox is trying to ",
# and has a period added to its end. Make sure to test in your locale.
autofill-edit-payment-method-os-prompt-macos = show stored payment method information
autofill-edit-payment-method-os-prompt-windows = { -brand-short-name } is trying to show stored payment method information. Confirm access to this Windows account below.
autofill-edit-payment-method-os-prompt-other = { -brand-short-name } is trying to show stored payment method information.

# The links lead users to Form Autofill browser preferences.
autofill-options-link = Form Autofill Options
autofill-options-link-osx = Form Autofill Preferences

## The credit card capture doorhanger

# If Sync is enabled and credit card sync is available,
# this checkbox is displayed on the doorhanger shown when saving credit card.
credit-card-doorhanger-credit-cards-sync-checkbox = Sync all saved cards across my devices

# Used on the doorhanger when users submit payment with credit card.
credit-card-save-doorhanger-header = Securely save this card?
credit-card-save-doorhanger-description = { -brand-short-name } encrypts your card number. Your security code won’t be saved.

credit-card-capture-save-button =
    .label = Save
    .accessKey = S
credit-card-capture-cancel-button =
    .label = Not now
    .accessKey = W
credit-card-capture-never-save-button =
    .label = Never save cards
    .accessKey = N

# Used on the doorhanger when an credit card change is detected.

credit-card-update-doorhanger-header = Update card?
credit-card-update-doorhanger-description = Card to update:

credit-card-capture-save-new-button =
    .label = Save as new card
    .accessKey = C
credit-card-capture-update-button =
    .label = Update existing card
    .accessKey = U
