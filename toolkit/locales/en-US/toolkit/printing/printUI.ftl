# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

printui-title = Print

# Variables
# $sheetCount (integer) - Number of paper sheets
printui-sheets-count =
    { $sheetCount ->
        [one] { $sheetCount } sheet of paper
       *[other] { $sheetCount } sheets of paper
    }

printui-page-range-all = All
printui-page-range-custom = Custom
printui-page-range-label = Pages
printui-page-range-picker =
  .aria-label = Pick page range
printui-page-custom-range =
  .aria-label = Enter custom page range

# Section title for the number of copies to print
printui-copies-label = Copies

printui-orientation = Orientation
printui-landscape = Landscape
printui-portrait = Portrait

# Section title for the printer or destination device to target
printui-destination-label = Destination

printui-more-settings = More settings
printui-less-settings = Fewer settings

# Section title (noun) for the print scaling options
printui-scale = Scale
printui-scale-fit-to-page = Fit to page
# Label for input control where user can set the scale percentage
printui-scale-pcent = Scale

# Section title for miscellaneous print options
printui-options = Options
printui-headers-footers-checkbox = Print headers and footers
printui-backgrounds-checkbox = Print backgrounds

printui-system-dialog-link = Print using the system dialog…

printui-primary-button = Print
printui-cancel-button = Cancel

