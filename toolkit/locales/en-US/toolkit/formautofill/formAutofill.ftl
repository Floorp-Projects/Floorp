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

# Label for the button in the dropdown menu used to clear the populated form.
autofill-clear-form-label = Clear Autofill Form

# Used as a label for the button, displayed at the bottom of the dropdown suggestion, to open Form Autofill browser preferences.
autofill-manage-addresses-label = Manage addresses

# Used as a label for the button, displayed at the bottom of the dropdown suggestion, to open Form Autofill browser preferences.
autofill-manage-payment-methods-label = Manage payment methods

## These are brand names and should only be translated when a locale-specific name for that brand is in common use

autofill-card-network-amex = American Express
autofill-card-network-cartebancaire = Carte Bancaire
autofill-card-network-diners = Diners Club
autofill-card-network-discover = Discover
autofill-card-network-jcb = JCB
autofill-card-network-mastercard = MasterCard
autofill-card-network-mir = MIR
autofill-card-network-unionpay = Union Pay
autofill-card-network-visa = Visa

# The warning text that is displayed for informing users what categories are
# about to be filled.  The text would be, for example,
#   Also autofills organization, phone, email.
# Variables:
#   $categories - one or more of the categories, see autofill-category-X below
autofill-phishing-warningmessage-extracategory = Also autofills { $categories }

# Variation when all are in the same category.
# Variables:
#   $categories - one or more of the categories
autofill-phishing-warningmessage = Autofills { $categories }

# Used in autofill drop down suggestion to indicate what other categories Form Autofill will attempt to fill.
autofill-category-address = address
autofill-category-name = name
autofill-category-organization = organization
autofill-category-tel = phone
autofill-category-email = email
