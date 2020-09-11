# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# This value isn't used directly, but is defined to avoid duplication
# in the "credit-card-label-*" strings.
#
# Variables:
#   $month (String): Numeric month the credit card expires
#   $year (String): Four-digit year the credit card expires
credit-card-expiration = Expires on { $month }/{ $year }

## These labels serve as a description of a credit card.
## The description must include a credit card number, and may optionally
## include a cardholder name, an expiration date, or both, so we have
## four variations.

# Label for a credit card with a number only
#
# Variables:
#   $number (String): Partially-redacted credit card number
#   $type (String): Credit card type
credit-card-label-number-2 = { $number }
    .aria-label = { $type } { credit-card-label-number-2 }

# Label for a credit card with a number and name
#
# Variables:
#   $number (String): Partially-redacted credit card number
#   $name (String): Cardholder name
#   $type (String): Credit card type
credit-card-label-number-name-2 = { $number }, { $name }
    .aria-label = { $type } { credit-card-label-number-name-2 }

# Label for a credit card with a number and expiration date
#
# Variables:
#   $number (String): Partially-redacted credit card number
#   $type (String): Credit card type
credit-card-label-number-expiration-2 = { $number }, { credit-card-expiration }
    .aria-label = { $type } { credit-card-label-number-expiration-2 }

# Label for a credit card with a number, name, and expiration date
#
# Variables:
#   $number (String): Partially-redacted credit card number
#   $name (String): Cardholder name
#   $type (String): Credit card type
credit-card-label-number-name-expiration-2 =
  { $number }, { $name }, { credit-card-expiration }
    .aria-label = { $type } { credit-card-label-number-name-expiration-2 }
