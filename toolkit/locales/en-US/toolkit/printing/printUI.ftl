# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

printui-title = Print
# Dialog title to prompt the user for a filename to save print to PDF.
printui-save-to-pdf-title = Save As

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
# This label is displayed before the first input field indicating
# the start of the range to print.
printui-range-start = From
# This label is displayed between the input fields indicating
# the start and end page of the range to print.
printui-range-end = to

# Section title for the number of copies to print
printui-copies-label = Copies

printui-orientation = Orientation
printui-landscape = Landscape
printui-portrait = Portrait

# Section title for the printer or destination device to target
printui-destination-label = Destination
printui-destination-pdf-label = Save to PDF

printui-more-settings = More settings
printui-less-settings = Fewer settings

# Section title (noun) for the print scaling options
printui-scale = Scale
printui-scale-fit-to-page-width = Fit to page width
# Label for input control where user can set the scale percentage
printui-scale-pcent = Scale

# Section title for miscellaneous print options
printui-options = Options
printui-headers-footers-checkbox = Print headers and footers
printui-backgrounds-checkbox = Print backgrounds

printui-color-mode-label = Color mode
printui-color-mode-color = Color
printui-color-mode-bw = Black and white

printui-margins = Margins
printui-margins-default = Default
printui-margins-min = Minimum
printui-margins-none = None

printui-system-dialog-link = Print using the system dialog…

printui-primary-button = Print
printui-primary-button-save = Save
printui-cancel-button = Cancel

printui-loading = Preparing Preview

## Error messages shown when a user has an invalid input

printui-error-invalid-scale = Scale must be a number between 10 and 200.

# Variables
# $numPages (integer) - Number of pages
printui-error-invalid-range = Range must be a number between 1 and { $numPages }.
printui-error-invalid-start-overflow = The “from” page number must be smaller than the “to” page number.
