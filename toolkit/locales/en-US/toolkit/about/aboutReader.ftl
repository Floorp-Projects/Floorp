# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

about-reader-loading = Loading…
about-reader-load-error = Failed to load article from page

about-reader-color-light-theme = Light
    .title = Light theme
about-reader-color-dark-theme = Dark
    .title = Dark theme
about-reader-color-sepia-theme = Sepia
    .title = Sepia theme
about-reader-color-auto-theme = Auto
    .title = Auto theme
about-reader-color-gray-theme = Gray
    .title = Gray theme
about-reader-color-contrast-theme = Contrast
    .title = Contrast theme

# An estimate for how long it takes to read an article,
# expressed as a range covering both slow and fast readers.
# Variables:
#   $rangePlural (String): The plural category of the range, using the same set as for numbers.
#   $range (String): The range of minutes as a localised string. Examples: "3-7", "~1".
about-reader-estimated-read-time =
    { $rangePlural ->
        [one] { $range } minute
       *[other] { $range } minutes
    }

## These are used as tooltips in Type Control

about-reader-toolbar-minus =
    .title = Decrease font size
about-reader-toolbar-plus =
    .title = Increase font size
about-reader-toolbar-contentwidthminus =
    .title = Decrease Content Width
about-reader-toolbar-contentwidthplus =
    .title = Increase Content Width
about-reader-toolbar-lineheightminus =
    .title = Decrease Line Height
about-reader-toolbar-lineheightplus =
    .title = Increase Line Height

## These are the styles of typeface that are options in the reader view controls.

about-reader-font-type-serif = Serif
about-reader-font-type-sans-serif = Sans-serif
about-reader-font-type-monospace = Monospace

## Reader View toolbar buttons

about-reader-toolbar-close = Close Reader View
about-reader-toolbar-type-controls = Type controls
about-reader-toolbar-text-layout-controls = Text and layout
about-reader-toolbar-theme-controls = Theme
about-reader-toolbar-savetopocket = Save to { -pocket-brand-name }

## Reader View colors menu

about-reader-colors-menu-header = Theme

about-reader-fxtheme-tab = Default
about-reader-customtheme-tab = Custom

## These are used as labels for the custom theme color pickers.
## The .title element is used to make the editing functionality
## clear and give context for screen reader users.

about-reader-custom-colors-foreground = Text
    .title = Edit color
about-reader-custom-colors-background = Background
    .title = Edit color

about-reader-custom-colors-unvisited-links = Unvisited links
    .title = Edit color
about-reader-custom-colors-visited-links = Visited links
    .title = Edit color

about-reader-custom-colors-selection-highlight = Highlighter for read aloud
    .title = Edit color

about-reader-reset-button = Reset defaults

## Reader View improved text and layout menu

about-reader-text-header = Text

about-reader-text-size-label = Text size
about-reader-font-type-selector-label = Font
about-reader-font-weight-selector-label = Font weight

about-reader-font-weight-light = Light
about-reader-font-weight-regular = Regular
about-reader-font-weight-bold = Bold

about-reader-layout-header = Layout

about-reader-slider-label-spacing-standard = Standard
about-reader-slider-label-spacing-wide = Wide

about-reader-content-width-label =
  .label = Content width
about-reader-line-spacing-label =
  .label = Line spacing

about-reader-advanced-layout-header = Advanced

about-reader-character-spacing-label =
  .label = Character spacing
about-reader-word-spacing-label =
  .label = Word spacing
about-reader-text-alignment-label = Text alignment
about-reader-text-alignment-left =
    .title = Align text left
about-reader-text-alignment-center =
    .title = Align text center
about-reader-text-alignment-right =
    .title = Align text right
