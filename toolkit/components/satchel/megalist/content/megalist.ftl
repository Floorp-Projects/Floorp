# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

filter-placeholder =
  .placeholder = Search Your Data
  .key = F

## Commands

command-copy = Copy
command-reveal = Reveal
command-conceal = Conceal
command-toggle = Toggle
command-open = Open
command-delete = Remove record
command-edit = Edit
command-save = Save
command-cancel = Cancel

## Passwords

passwords-section-label = Passwords
passwords-disabled = Passwords are disabled

passwords-command-create = Add Password
passwords-command-import = Import from a File…
passwords-command-export = Export Passwords…
passwords-command-remove-all = Remove All Passwords…
passwords-command-settings = Settings
passwords-command-help = Help

passwords-import-file-picker-title = Import Passwords
passwords-import-file-picker-import-button = Import

# A description for the .csv file format that may be shown as the file type
# filter by the operating system.
passwords-import-file-picker-csv-filter-title =
  { PLATFORM() ->
      [macos] CSV Document
     *[other] CSV File
  }
# A description for the .tsv file format that may be shown as the file type
# filter by the operating system. TSV is short for 'tab separated values'.
passwords-import-file-picker-tsv-filter-title =
  { PLATFORM() ->
      [macos] TSV Document
     *[other] TSV File
  }

# Variables
#   $count (number) - Number of passwords
passwords-count =
  { $count ->
      [one] { $count } password
     *[other] { $count } passwords
  }

# Variables
#   $count (number) - Number of filtered passwords
#   $total (number) - Total number of passwords
passwords-filtered-count =
  { $total ->
      [one] { $count } of { $total } password
     *[other] { $count } of { $total } passwords
  }

passwords-origin-label = Website address
passwords-username-label = Username
passwords-password-label = Password

## Payments

payments-command-create = Add Payment Method

payments-section-label = Payment methods
payments-disabled = Payments methods are disabled

# Variables
#   $count (number) - Number of payment methods
payments-count =
  { $count ->
      [one] { $count } payment method
     *[other] { $count } payment methods
  }

# Variables
#   $count (number) - Number of filtered payment methods
#   $total (number) - Total number of payment methods
payments-filtered-count =
  { $total ->
      [one] { $count } of { $total } payment method
     *[other] { $count } of { $total } payment methods
  }

card-number-label = Card Number
card-expiration-label = Expires on
card-holder-label = Name on Card

## Addresses

addresses-command-create = Add Address

addresses-section-label = Addresses
addresses-disabled = Addresses are disabled

# Variables
#   $count (number) - Number of addresses
addresses-count =
  { $count ->
      [one] { $count } address
     *[other] { $count } addresses
  }

# Variables
#   $count (number) - Number of filtered addresses
#   $total (number) - Total number of addresses
addresses-filtered-count =
  { $total ->
      [one] { $count } of { $total } address
     *[other] { $count } of { $total } addresses
  }

address-name-label = Name
address-phone-label = Phone
address-email-label = Email
