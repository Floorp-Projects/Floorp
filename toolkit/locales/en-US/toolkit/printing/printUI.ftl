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
printui-page-custom-range-input =
  .aria-label = Enter custom page range
  .placeholder = e.g. 2-6, 9, 12-16

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

printui-paper-size-label = Paper size

# Section title (noun) for the print scaling options
printui-scale = Scale
printui-scale-fit-to-page-width = Fit to page width
# Label for input control where user can set the scale percentage
printui-scale-pcent = Scale

# Section title (noun) for the two-sided print options
printui-two-sided-printing = Two-sided printing
printui-two-sided-printing-off = Off
# Flip the sheet as if it were bound along its long edge.
printui-two-sided-printing-long-edge = Flip on long edge
# Flip the sheet as if it were bound along its short edge.
printui-two-sided-printing-short-edge = Flip on short edge

# Section title for miscellaneous print options
printui-options = Options
printui-headers-footers-checkbox = Print headers and footers
printui-backgrounds-checkbox = Print backgrounds
printui-selection-checkbox = Print selection only

printui-color-mode-label = Color mode
printui-color-mode-color = Color
printui-color-mode-bw = Black and white

printui-margins = Margins
printui-margins-default = Default
printui-margins-min = Minimum
printui-margins-none = None
printui-margins-custom-inches = Custom (inches)
printui-margins-custom-mm = Custom (mm)
printui-margins-custom-top = Top
printui-margins-custom-top-inches = Top (inches)
printui-margins-custom-top-mm = Top (mm)
printui-margins-custom-bottom = Bottom
printui-margins-custom-bottom-inches = Bottom (inches)
printui-margins-custom-bottom-mm = Bottom (mm)
printui-margins-custom-left = Left
printui-margins-custom-left-inches = Left (inches)
printui-margins-custom-left-mm = Left (mm)
printui-margins-custom-right = Right
printui-margins-custom-right-inches = Right (inches)
printui-margins-custom-right-mm = Right (mm)

printui-system-dialog-link = Print using the system dialog…

printui-primary-button = Print
printui-primary-button-save = Save
printui-cancel-button = Cancel
printui-close-button = Close

printui-loading = Preparing Preview

# Reported by screen readers and other accessibility tools to indicate that
# the print preview has focus.
printui-preview-label =
    .aria-label = Print Preview

printui-pages-per-sheet = Pages per sheet

# This is shown next to the Print button with an indefinite loading spinner
# when the user prints a page and it is being sent to the printer.
printui-print-progress-indicator = Printing…
printui-print-progress-indicator-saving = Saving…

## Paper sizes that may be supported by the Save to PDF destination:

printui-paper-a5 = A5
printui-paper-a4 = A4
printui-paper-a3 = A3
printui-paper-a2 = A2
printui-paper-a1 = A1
printui-paper-a0 = A0
printui-paper-b5 = B5
printui-paper-b4 = B4
printui-paper-jis-b5 = JIS-B5
printui-paper-jis-b4 = JIS-B4
printui-paper-letter = US Letter
printui-paper-legal = US Legal
printui-paper-tabloid = Tabloid

## Error messages shown when a user has an invalid input

printui-error-invalid-scale = Scale must be a number between 10 and 200.
printui-error-invalid-margin = Please enter a valid margin for the selected paper size.
printui-error-invalid-copies = Copies must be a number between 1 and 10000.

# Variables
# $numPages (integer) - Number of pages
printui-error-invalid-range = Range must be a number between 1 and { $numPages }.
printui-error-invalid-start-overflow = The “from” page number must be smaller than the “to” page number.
