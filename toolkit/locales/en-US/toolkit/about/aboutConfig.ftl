# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config-window =
    .title = about:config

## Strings used to display a warning in about:config

# This text should be attention grabbing and playful
config-about-warning-title =
    .value = This might void your warranty!
config-about-warning-text = Changing these advanced settings can be harmful to the stability, security, and performance of this application. You should only continue if you are sure of what you are doing.
config-about-warning-button =
    .label = I accept the risk!
config-about-warning-checkbox =
    .label = Show this warning next time

config-search-prefs =
    .value = Search:
    .accesskey = r

config-focus-search =
    .key = r

config-focus-search-2 =
    .key = f

## These strings are used for column headers
config-pref-column =
    .label = Preference Name
config-lock-column =
    .label = Status
config-type-column =
    .label = Type
config-value-column =
    .label = Value

## These strings are used for tooltips
config-pref-column-header =
    .tooltip = Click to sort
config-column-chooser =
    .tooltip = Click to select columns to display

## These strings are used for the context menu
config-copy-pref =
    .key = C
    .label = Copy
    .accesskey = C

config-copy-name =
    .label = Copy Name
    .accesskey = N

config-copy-value =
    .label = Copy Value
    .accesskey = V

config-modify =
    .label = Modify
    .accesskey = M

config-toggle =
    .label = Toggle
    .accesskey = T

config-reset =
    .label = Reset
    .accesskey = R

config-new =
    .label = New
    .accesskey = w

config-string =
    .label = String
    .accesskey = S

config-integer =
    .label = Integer
    .accesskey = I

config-boolean =
    .label = Boolean
    .accesskey = B

config-default = default
config-modified = modified
config-locked = locked

config-property-string = string
config-property-int = integer
config-property-bool = boolean

config-new-prompt = Enter the preference name

config-nan-title = Invalid value
config-nan-text = The text you entered is not a number.

# Variables:
#   $type (String): type of value (boolean, integer or string)
config-new-title = New { $type } value

# Variables:
#   $type (String): type of value (boolean, integer or string)
config-modify-title = Enter { $type } value
